// Copyright 2022-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  System builder for daemon headsets
 * @author Joseph Albers <joseph.albers@outlook.de>
 * @ingroup xrt_iface
 */

#include "math/m_api.h"
#include "math/m_space.h"
#include "multi_wrapper/multi.h"
#include "realsense/rs_interface.h"
#include "tracking/t_hand_tracking.h"
#include "tracking/t_tracking.h"

#include "xrt/xrt_config_drivers.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_prober.h"

#include "util/u_builders.h"
#include "util/u_config_json.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_sink.h"
#include "util/u_system_helpers.h"
#include "util/u_file.h"
#include "util/u_pretty_print.h"

#include "target_builder_interface.h"

#include "daemon/daemon_interface.h"

#ifdef XRT_BUILD_DRIVER_ULV2
#include "ultraleap_v2/ulv2_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_REALSENSE
#include "realsense/rs_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_DEPTHAI
#include "depthai/depthai_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_TWRAP
#include "twrap/twrap_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_HANDTRACKING
#include "ht/ht_interface.h"
#endif

#ifdef XRT_BUILD_DRIVER_STEREOLABS
#include "stereolabs/sl_interface.h"
#endif

#include "ht_ctrl_emu/ht_ctrl_emu_interface.h"

#include "xrt/xrt_frameserver.h"
#include "xrt/xrt_results.h"
#include "xrt/xrt_tracking.h"

#include <assert.h>
#include "math/m_mathinclude.h"

DEBUG_GET_ONCE_OPTION(daemon_config_path, "DAEMON_CONFIG_PATH", NULL)
DEBUG_GET_ONCE_LOG_OPTION(daemon_log, "DAEMON_LOG", U_LOGGING_WARN)

#define DAEMON_TRACE(...) U_LOG_IFL_T(debug_get_log_option_daemon_log(), __VA_ARGS__)
#define DAEMON_DEBUG(...) U_LOG_IFL_D(debug_get_log_option_daemon_log(), __VA_ARGS__)
#define DAEMON_INFO(...) U_LOG_IFL_I(debug_get_log_option_daemon_log(), __VA_ARGS__)
#define DAEMON_WARN(...) U_LOG_IFL_W(debug_get_log_option_daemon_log(), __VA_ARGS__)
#define DAEMON_ERROR(...) U_LOG_IFL_E(debug_get_log_option_daemon_log(), __VA_ARGS__)

static const char *driver_list[] = {
    "daemon hmd",
};

struct daemon_realsense_t26x
{
	bool active;
	//! @todo make P_middleofeyes_to_trackingcenter_oxr settable by the user/config file
	struct xrt_pose P_middleofeyes_to_trackingcenter_oxr;
};

struct daemon_stereolabs_zed_mini
{
	bool active;
	//! @todo make P_middleofeyes_to_trackingcenter_oxr settable by the user/config file
	struct xrt_pose P_middleofeyes_to_trackingcenter_oxr;
};

struct daemon_ultraleap_device
{
	bool active;
	// Users input `P_middleofeyes_to_trackingcenter_oxr`, and we invert it into this pose.
	// It's a lot simpler to (and everybody does) care about the transform from the eyes center to the device,
	// but tracking overrides care about this value.
	//! @todo make P_trackingcenter_to_middleofeyes_oxr settable by the user/config file
	struct xrt_pose P_trackingcenter_to_middleofeyes_oxr;
};

struct daemon_depthai_device
{
	bool active;
	//! @todo make P_middleofeyes_to_imu_oxr settable by the user/config file
	struct xrt_pose P_middleofeyes_to_imu_oxr;
};

struct daemon_builder
{
	struct xrt_builder base;

	const char *config_path;
	cJSON *config_json;

	struct daemon_realsense_t26x realsense_t26x;
	struct daemon_stereolabs_zed_mini stereolabs_device;
	struct daemon_ultraleap_device ultraleap_device;
	struct daemon_depthai_device depthai_device;
};

#if defined(XRT_BUILD_DRIVER_DEPTHAI) && defined(XRT_BUILD_DRIVER_HANDTRACKING)
static xrt_result_t
daemon_setup_depthai_device(struct daemon_builder *db,
                            struct u_system_devices *usysd,
                            struct xrt_device **out_hand_device)
{
	struct depthai_slam_startup_settings settings = {0};
	xrt_result_t xret;

	settings.frames_per_second = 60;
	settings.half_size_ov9282 = true;
	settings.want_cameras = true;
	settings.want_imu = true;

	struct xrt_fs *the_fs = depthai_fs_slam(&usysd->xfctx, &settings);

	if (the_fs == NULL) {
		return XRT_ERROR_DEVICE_CREATION_FAILED;
	}

	struct t_stereo_camera_calibration *calib = NULL;
	depthai_fs_get_stereo_calibration(the_fs, &calib);

	struct xrt_slam_sinks *hand_sinks = NULL;

	struct t_camera_extra_info extra_camera_info = {0};

	extra_camera_info.views[0].camera_orientation = CAMERA_ORIENTATION_0;
	extra_camera_info.views[1].camera_orientation = CAMERA_ORIENTATION_0;

	extra_camera_info.views[0].boundary_type = HT_IMAGE_BOUNDARY_NONE;
	extra_camera_info.views[1].boundary_type = HT_IMAGE_BOUNDARY_NONE;

	int create_status = ht_device_create(&usysd->xfctx,     //
	                                     calib,             //
	                                     extra_camera_info, //
	                                     &hand_sinks,       //
	                                     out_hand_device);
	t_stereo_camera_calibration_reference(&calib, NULL);
	if (create_status != 0) {
		return XRT_ERROR_DEVICE_CREATION_FAILED;
	}

	struct xrt_slam_sinks entry_sinks = {0};

	entry_sinks = (struct xrt_slam_sinks){
	    .cams[0] = hand_sinks->cams[0],
	    .cams[1] = hand_sinks->cams[1],
	};

	struct xrt_slam_sinks dummy_slam_sinks = {0};

	u_sink_force_genlock_create(&usysd->xfctx, entry_sinks.cams[0], entry_sinks.cams[1], &dummy_slam_sinks.cams[0],
	                            &dummy_slam_sinks.cams[1]);

	xrt_fs_slam_stream_start(the_fs, &dummy_slam_sinks);

	return XRT_SUCCESS;
}
#endif

static xrt_result_t
daemon_estimate_system(struct xrt_builder *xb,
                       cJSON *config,
                       struct xrt_prober *xp,
                       struct xrt_builder_estimate *estimate)
{
	struct daemon_builder *db = (struct daemon_builder *)xb;
	U_ZERO(estimate);

	struct xrt_prober_device **xpdevs = NULL;
	size_t xpdev_count = 0;
	xrt_result_t xret = XRT_SUCCESS;

	// Lock the device list
	xret = xrt_prober_lock_list(xp, &xpdevs, &xpdev_count);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	bool head_tracking_6dof = false;
	bool hand_tracking = false;

// HEAD
#ifdef XRT_BUILD_DRIVER_REALSENSE
	head_tracking_6dof =
	    head_tracking_6dof || u_builder_find_prober_device(xpdevs, xpdev_count, REALSENSE_MOVIDIUS_VID,
	                                                       REALSENSE_MOVIDIUS_PID, XRT_BUS_TYPE_USB);
	head_tracking_6dof = head_tracking_6dof || u_builder_find_prober_device(xpdevs, xpdev_count,                  //
	                                                                        REALSENSE_TM2_VID, REALSENSE_TM2_PID, //
	                                                                        XRT_BUS_TYPE_USB);
#endif
#ifdef XRT_BUILD_DRIVER_STEREOLABS
	head_tracking_6dof = head_tracking_6dof ||
	                     u_builder_find_prober_device(xpdevs, xpdev_count, SLZM_VID, SLZM_PID, XRT_BUS_TYPE_USB);
#endif
	estimate->certain.head = head_tracking_6dof;

// HANDS
#ifdef XRT_BUILD_DRIVER_ULV2
	hand_tracking =
	    hand_tracking || u_builder_find_prober_device(xpdevs, xpdev_count, ULV2_VID, ULV2_PID, XRT_BUS_TYPE_USB);
#endif
#if defined(XRT_BUILD_DRIVER_DEPTHAI) && defined(XRT_BUILD_DRIVER_HANDTRACKING)
	hand_tracking = hand_tracking ||
	                u_builder_find_prober_device(xpdevs, xpdev_count, DEPTHAI_VID, DEPTHAI_PID, XRT_BUS_TYPE_USB);
	;
#endif
	estimate->certain.left = estimate->certain.right = estimate->maybe.left = estimate->maybe.right = hand_tracking;

	xret = xrt_prober_unlock_list(xp, &xpdevs);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	return XRT_SUCCESS;
}

static xrt_result_t
daemon_open_system(struct xrt_builder *xb,
                   cJSON *config,
                   struct xrt_prober *xp,
                   struct xrt_system_devices **out_xsysd,
                   struct xrt_space_overseer **out_xso)
{
	struct daemon_builder *db = (struct daemon_builder *)xb;

	struct u_system_devices *usysd = u_system_devices_allocate();
	xrt_result_t result = XRT_SUCCESS;

	if (out_xsysd == NULL || *out_xsysd != NULL) {
		DAEMON_ERROR("Invalid output system pointer");
		result = XRT_ERROR_DEVICE_CREATION_FAILED;
		goto end;
	}

	struct xrt_device *db_hmd = daemon_hmd_create(db->config_json);
	if (db_hmd == NULL) {
		result = XRT_ERROR_DEVICE_CREATION_FAILED;
		goto end;
	}

	db->realsense_t26x.active = false;
	db->stereolabs_device.active = true;
	db->ultraleap_device.active = true;
	db->depthai_device.active = false;

	struct xrt_device *hand_device = NULL;
	struct xrt_device *slam_device = NULL;

	struct xrt_pose head_offset = XRT_POSE_IDENTITY;

	// True if hand tracker is parented to the head tracker (DepthAI), false if hand tracker is parented to
	// middle-of-eyes (Ultraleap etc.)
	bool hand_parented_to_head_tracker = true;
	struct xrt_pose hand_offset = XRT_POSE_IDENTITY;

	// HEAD
	if (db->realsense_t26x.active) {
#ifdef XRT_BUILD_DRIVER_REALSENSE
		slam_device = rs_create_tracked_device_internal_slam();
		head_offset = db->realsense_t26x.P_middleofeyes_to_trackingcenter_oxr;
#endif
	} else if (db->stereolabs_device.active) {
#ifdef XRT_BUILD_DRIVER_STEREOLABS
		slam_device = sl_zed_mini_create();
		head_offset = db->stereolabs_device.P_middleofeyes_to_trackingcenter_oxr;
#endif
	}

	// HANDS
	if (db->ultraleap_device.active) {
#ifdef XRT_BUILD_DRIVER_ULV2
		ulv2_create_device(&hand_device);
		hand_offset = db->ultraleap_device.P_trackingcenter_to_middleofeyes_oxr;
		hand_parented_to_head_tracker = false;
#endif
	} else if (db->depthai_device.active) {
#if defined(XRT_BUILD_DRIVER_DEPTHAI) && defined(XRT_BUILD_DRIVER_HANDTRACKING)
		daemon_setup_depthai_device(db, usysd, &hand_device);
		head_offset = db->depthai_device.P_middleofeyes_to_imu_oxr;
		hand_parented_to_head_tracker = true;
#endif
	}

	struct xrt_device *head_wrap = NULL;

	// wrap the tracked pose function of daemon driver into the t26x/zed mini tracked pose function
	if (slam_device != NULL) {
		usysd->base.xdevs[usysd->base.xdev_count++] = slam_device;
		head_wrap = multi_create_tracking_override(XRT_TRACKING_OVERRIDE_DIRECT, db_hmd, slam_device,
		                                           XRT_INPUT_GENERIC_TRACKER_POSE, &head_offset);

		usysd->base.xdevs[usysd->base.xdev_count++] = head_wrap;
		usysd->base.roles.head = head_wrap;
	}

	if (hand_device != NULL) {
		// note: hand_parented_to_head_tracker is always false when slam_device is NULL
		struct xrt_device *hand_wrap = multi_create_tracking_override(
		    XRT_TRACKING_OVERRIDE_ATTACHED, hand_device,
		    hand_parented_to_head_tracker ? slam_device : head_wrap,
		    hand_parented_to_head_tracker ? XRT_INPUT_GENERIC_TRACKER_POSE : XRT_INPUT_GENERIC_HEAD_POSE,
		    &hand_offset);
		struct xrt_device *two_hands[2];
		cemu_devices_create(head_wrap, hand_wrap, two_hands);

		usysd->base.xdevs[usysd->base.xdev_count++] = two_hands[0];
		usysd->base.xdevs[usysd->base.xdev_count++] = two_hands[1];

		usysd->base.roles.hand_tracking.left = two_hands[0];
		usysd->base.roles.hand_tracking.right = two_hands[1];

		usysd->base.roles.left = two_hands[0];
		usysd->base.roles.right = two_hands[1];
	}

end:
	if (result == XRT_SUCCESS) {
		*out_xsysd = &usysd->base;
		u_builder_create_space_overseer(&usysd->base, out_xso);
	} else {
		u_system_devices_destroy(&usysd);
	}
	if (db->config_json != NULL) {
		cJSON_Delete(db->config_json);
	}

	return result;
}

static void
daemon_destroy(struct xrt_builder *xb)
{
	free(xb);
}

/*
 *
 * 'Exported' functions.
 *
 */

struct xrt_builder *
t_builder_daemon_create(void)
{
	struct daemon_builder *db = U_TYPED_CALLOC(struct daemon_builder);
	db->base.estimate_system = daemon_estimate_system;
	db->base.open_system = daemon_open_system;
	db->base.destroy = daemon_destroy;
	db->base.identifier = "daemon";
	db->base.name = "daemon headset";
	db->base.driver_identifiers = driver_list;
	db->base.driver_identifier_count = ARRAY_SIZE(driver_list);

	return &db->base;
}

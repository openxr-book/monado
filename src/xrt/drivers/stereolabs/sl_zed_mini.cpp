// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Sample HMD device, use as a starting point to make your own device driver.
 *
 *
 * Based largely on simulated_hmd.c
 *
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup drv_sample
 */

#include "xrt/xrt_device.h"
#include "math/m_relation_history.h"
/*

#include "math/m_api.h"
#include "math/m_mathinclude.h"
*/

#include "os/os_time.h"
#include "os/os_threading.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_time.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_distortion_mesh.h"

#include "sl_interface.h"

#include "sl/Camera.hpp"

#include <stdio.h>


/*
 *
 * Structs and defines.
 *
 */

DEBUG_GET_ONCE_LOG_OPTION(stereolabs_log, "STEREOLABS_LOG", U_LOGGING_WARN)

#define SL_TRACE(sl, ...) U_LOG_XDEV_IFL_T(&sl->base, sl->log_level, __VA_ARGS__)
#define SL_DEBUG(sl, ...) U_LOG_XDEV_IFL_D(&sl->base, sl->log_level, __VA_ARGS__)
#define SL_ERROR(sl, ...) U_LOG_XDEV_IFL_E(&sl->base, sl->log_level, __VA_ARGS__)

struct sl_zed_mini
{
	struct xrt_device base;

	struct m_relation_history *relation_hist;

	struct os_thread_helper oth;

	enum u_logging_level log_level;

	sl::Camera *camera;
};

/// Casting helper function
static inline struct sl_zed_mini *
sl_zed_mini(struct xrt_device *xdev)
{
	return (struct sl_zed_mini *)xdev;
}

/*!
 * A sample HMD device.
 *
 * @implements xrt_device
 */

/*!
 * Create all Stereolabs resources needed for 6DOF tracking.
 */
static int
create_zed_mini_device(struct sl_zed_mini *sl_zm)
{
	SL_DEBUG(sl_zm, "creating device\n");
	sl::InitParameters init_p;
	init_p.camera_resolution = sl::RESOLUTION::AUTO;
	init_p.coordinate_system = sl::COORDINATE_SYSTEM::RIGHT_HANDED_Y_UP;
	init_p.coordinate_units = sl::UNIT::METER;
	init_p.sensors_required = true;
	init_p.async_grab_camera_recovery = true;

	if (sl_zm->camera->open(init_p) != sl::ERROR_CODE::SUCCESS) {
		SL_ERROR(sl_zm, "no zed camera connected!!\n");
		return -1;
	}

	sl::PositionalTrackingParameters tracking_p;
	if (sl_zm->camera->enablePositionalTracking(tracking_p) != sl::ERROR_CODE::SUCCESS) {
		SL_ERROR(sl_zm, "couldn't enable positional tracking!!\n");
		return -1;
	}

	bool has_imu =
	    sl_zm->camera->getCameraInformation().sensors_configuration.isSensorAvailable(sl::SENSOR_TYPE::GYROSCOPE);

	return 0;
}

static int
update(struct sl_zed_mini *sl_zm)
{
	sl::SensorsData sensor_d;
	sl::Pose pose_d;

	if (sl_zm->camera->grab() == sl::ERROR_CODE::SUCCESS) {

		// TIME_REFERENCE::IMAGE is synchronized to the image frame
		// TIME_REFERENCE::CURRENT is synchronized to the function call time
		sl_zm->camera->getSensorsData(sensor_d, sl::TIME_REFERENCE::IMAGE);
		sl::float3 angular_velocity = sensor_d.imu.angular_velocity;

		// REFERENCE_FRAME::WORLD is transform with reference to world frame
		// REFERENCE_FRAME::CAMERA is transform with reference to previous camera frame
		sl_zm->camera->getPosition(pose_d, sl::REFERENCE_FRAME::WORLD);
		sl::Translation translation = pose_d.getTranslation();
		sl::Orientation orientation = pose_d.getOrientation();

		uint64_t timestamp_ns = pose_d.timestamp.getNanoseconds();
		uint64_t now_real_ns = os_realtime_get_ns();
		uint64_t now_monotonic_ns = os_monotonic_get_ns();

		uint64_t diff_ns = now_real_ns - timestamp_ns;
		timestamp_ns = now_monotonic_ns - diff_ns;

		struct xrt_space_relation relation;

		// Rotation
		relation.pose.orientation.x = orientation.x;
		relation.pose.orientation.y = orientation.y;
		relation.pose.orientation.z = orientation.z;
		relation.pose.orientation.w = orientation.w;
		// relation.angular_velocity.x = angular_velocity.x;
		// relation.angular_velocity.y = angular_velocity.y;
		// relation.angular_velocity.z = angular_velocity.z;

		// Position divided by 100 because it feels right, aka check back later
		relation.pose.position.x = translation.x / 100;
		relation.pose.position.y = translation.y / 100;
		relation.pose.position.z = translation.z / 100;

		relation.relation_flags = (enum xrt_space_relation_flags)(
		    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
		    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT | XRT_SPACE_RELATION_POSITION_TRACKED_BIT);
		    // XRT_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT);

		m_relation_history_push(sl_zm->relation_hist, &relation, timestamp_ns);
	}

	return 0;
}

static void *
sl_run_thread(void *ptr)
{
	struct sl_zed_mini *sl_zm = (struct sl_zed_mini *)ptr;

	os_thread_helper_lock(&sl_zm->oth);

	while (os_thread_helper_is_running_locked(&sl_zm->oth)) {

		os_thread_helper_unlock(&sl_zm->oth);

		int ret = update(sl_zm);
		if (ret < 0) {
			return NULL;
		}
	}

	return NULL;
}

static void
sl_zed_mini_update_inputs(struct xrt_device *xdev)
{
	// Empty
}

static void
sl_zed_mini_get_tracked_pose(struct xrt_device *xdev,
                             enum xrt_input_name name,
                             uint64_t at_timestamp_ns,
                             struct xrt_space_relation *out_relation)
{
	struct sl_zed_mini *sl_zm = sl_zed_mini(xdev);

	if (name != XRT_INPUT_GENERIC_TRACKER_POSE) {
		SL_ERROR(sl_zm, "unknown input name");
		return;
	}

	m_relation_history_get(sl_zm->relation_hist, at_timestamp_ns, out_relation);
}

static void
sl_zed_mini_get_view_poses(struct xrt_device *xdev,
                           const struct xrt_vec3 *default_eye_relation,
                           uint64_t at_timestamp_ns,
                           uint32_t view_count,
                           struct xrt_space_relation *out_head_relation,
                           struct xrt_fov *out_fovs,
                           struct xrt_pose *out_poses)
{
	assert(false);
}

static void
sl_zed_mini_destroy(struct xrt_device *xdev)
{
	struct sl_zed_mini *sl_zm = sl_zed_mini(xdev);

	// 1. lock mutex and shutdown thread
	os_thread_helper_destroy(&sl_zm->oth);

	// 2. shutdown camera
	sl_zm->camera->disablePositionalTracking();
	sl_zm->camera->close();

	m_relation_history_destroy(&sl_zm->relation_hist);
	u_device_free(&sl_zm->base);
}

struct xrt_device *
sl_zed_mini_create(void)
{
	enum u_device_alloc_flags flags = U_DEVICE_ALLOC_TRACKING_NONE;
	struct sl_zed_mini *sl_zm = U_DEVICE_ALLOCATE(struct sl_zed_mini, flags, 1, 0);
	sl_zm->camera = new sl::Camera();

	m_relation_history_create(&sl_zm->relation_hist);

	sl_zm->log_level = debug_get_log_option_stereolabs_log();

	sl_zm->base.update_inputs = sl_zed_mini_update_inputs;
	sl_zm->base.get_tracked_pose = sl_zed_mini_get_tracked_pose;
	sl_zm->base.get_view_poses = sl_zed_mini_get_view_poses;
	sl_zm->base.destroy = sl_zed_mini_destroy;
	sl_zm->base.name = XRT_DEVICE_STEREOLABS;
	sl_zm->base.tracking_origin->type = XRT_TRACKING_TYPE_OTHER;
	sl_zm->base.tracking_origin->offset = (struct xrt_pose)XRT_POSE_IDENTITY;

	snprintf(sl_zm->base.str, XRT_DEVICE_NAME_LEN, "Stereolabs Slam");
	snprintf(sl_zm->base.serial, XRT_DEVICE_NAME_LEN, "Stereolabs Slam");

	sl_zm->base.inputs[0].name = XRT_INPUT_GENERIC_TRACKER_POSE;

	sl_zm->base.device_type = XRT_DEVICE_TYPE_GENERIC_TRACKER;
	sl_zm->base.orientation_tracking_supported = true;
	sl_zm->base.position_tracking_supported = true;

	int ret = 0;

	// Thread and other state.
	ret = os_thread_helper_init(&sl_zm->oth);
	if (ret != 0) {
		SL_ERROR(sl_zm, "Failed to init threading!");
		sl_zed_mini_destroy(&sl_zm->base);
		return NULL;
	}

	ret = create_zed_mini_device(sl_zm);
	if (ret != 0) {
		sl_zed_mini_destroy(&sl_zm->base);
		return NULL;
	}

	ret = os_thread_helper_start(&sl_zm->oth, sl_run_thread, sl_zm);
	if (ret != 0) {
		SL_ERROR(sl_zm, "Failed to start thread!");
		sl_zed_mini_destroy(&sl_zm->base);
		return NULL;
	}

	return &sl_zm->base;
}

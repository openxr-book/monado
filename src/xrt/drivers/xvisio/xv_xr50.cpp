// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  XVisio XR50 device driver.
 *
 * @author Joseph Albers <joseph.albers@outlook.de>
 * @ingroup drv_xvisio
 */

#include "xrt/xrt_device.h"
#include "math/m_relation_history.h"
#include "math/m_mathinclude.h"
#include "math/m_api.h"

#include "os/os_time.h"
#include "os/os_threading.h"

#include "util/u_sink.h"
#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_time.h"
#include "util/u_debug.h"
#include "util/u_frame.h"
#include "util/u_device.h"
#include "util/u_format.h"
#include "util/u_distortion_mesh.h"

#include "xv_interface.h"

#include "xv-sdk.h"

#include <stdio.h>
#include <iostream>

DEBUG_GET_ONCE_LOG_OPTION(xvisio_xr50_log, "XVISIO_XR50_LOG", U_LOGGING_WARN)
DEBUG_GET_ONCE_LOG_OPTION(xvisio_xr50_fs_log, "XVISIO_XR50_FS_LOG", U_LOGGING_WARN)

#define XV_XR50_TRACE(xvisio, ...) U_LOG_XDEV_IFL_T(&xvisio->base, xvisio->log_level, __VA_ARGS__)
#define XV_XR50_DEBUG(xvisio, ...) U_LOG_XDEV_IFL_D(&xvisio->base, xvisio->log_level, __VA_ARGS__)
#define XV_XR50_ERROR(xvisio, ...) U_LOG_XDEV_IFL_E(&xvisio->base, xvisio->log_level, __VA_ARGS__)

#define DEG_TO_RAD(D) ((D)*M_PI / 180.)

struct xvisio_xr50
{
	struct xrt_device base;

	struct m_relation_history *relation_hist;

	struct os_thread_helper oth;

	enum u_logging_level log_level;

	int fisheye_left_id;
	int fisheye_right_id;
    std::shared_ptr<xv::Device> xr50;
};

static inline struct xvisio_xr50 *
xv_xr50(struct xrt_device *xdev)
{
	return (struct xvisio_xr50 *)xdev;
}

void
fisheye_left_callback(xv::FisheyeImages const& fisheye) {
    static int k = 0;
    if (k++ % 50 == 0 && fisheye.images.size() >= 1) {
		// XV_XR50_DEBUG(xv_xr50, "%dx%d\n", fisheye.images.at(0).width, fisheye.images.at(0).height);
    }
}

void
fisheye_right_callback(xv::FisheyeImages const& fisheye) {
    static int k = 0;
    if (k++ % 50 == 0 && fisheye.images.size() >= 2) {
		// XV_XR50_DEBUG(xv_xr50, "%dx%d\n", fisheye.images.at(0).width, fisheye.images.at(0).height);
    }
}

	#include <thread>
static void
xvisio_xr50_destroy(struct xrt_device *xdev)
{
	struct xvisio_xr50 *xvisio_xr50 = xv_xr50(xdev);
	XV_XR50_DEBUG(xvisio_xr50, "Destroying XVisio XR50.");

	// 1. lock mutex and shutdown thread
	os_thread_helper_destroy(&xvisio_xr50->oth);

	// 2. shutdown device
	xvisio_xr50->xr50->slam()->stop();
	// xvisio_xr50->xr50->fisheyeCameras()->unregisterCallback(xvisio_xr50->fisheye_left_id);
	// xvisio_xr50->xr50->fisheyeCameras()->unregisterCallback(xvisio_xr50->fisheye_right_id);
	// xvisio_xr50->xr50->fisheyeCameras()->stop();

	m_relation_history_destroy(&xvisio_xr50->relation_hist);
	u_device_free(&xvisio_xr50->base);
}

static int
push_position_and_orientation(struct xvisio_xr50 *xv_xr50, struct xrt_quat *rotation_quat)
{
	xv::Pose pose;
	// need to experiment with this, just took the value from the xvisio demo code
	double prediction = 0.005;

	bool ok = xv_xr50->xr50->slam()->getPose(pose, prediction);

	if (ok) {
		std::int64_t timestamp_edge_ns = pose.edgeTimestampUs()*1000;
		std::array<double,4> orientation_quat = pose.quaternion(); // [qx,qy,qz,qw]
		std::array<double,3> angular_velocity = pose.angularVelocity();
		std::array<double,3> translation = pose.translation();
		std::array<double,3> linear_velocity = pose.linearVelocity();
		
		uint64_t timestamp_ns = timestamp_edge_ns;
		uint64_t now_real_ns = os_realtime_get_ns();
		uint64_t now_monotonic_ns = os_monotonic_get_ns();

		uint64_t diff_ns = now_real_ns - timestamp_ns;
		timestamp_ns = now_monotonic_ns - diff_ns;

		struct xrt_space_relation relation;

		struct xrt_quat xr50_orientation;
		xr50_orientation.x = orientation_quat[0];
		xr50_orientation.y = orientation_quat[1];
		xr50_orientation.z = orientation_quat[2];
		xr50_orientation.w = orientation_quat[3];

		struct xrt_quat corrected_coord_system_quat;
		math_quat_rotate(&xr50_orientation, rotation_quat, &corrected_coord_system_quat);

 		// Rotation
		relation.pose.orientation.x = corrected_coord_system_quat.x;
		relation.pose.orientation.y = -corrected_coord_system_quat.y;
		relation.pose.orientation.z = -corrected_coord_system_quat.z;
		relation.pose.orientation.w = corrected_coord_system_quat.w;
		// relation.angular_velocity.x = angular_velocity[0];
		// relation.angular_velocity.y = angular_velocity[1];
		// relation.angular_velocity.z = angular_velocity[2];

 		// Position
		relation.pose.position.x = translation[0];
		relation.pose.position.y = -translation[1];
		relation.pose.position.z = -translation[2];
		// relation.linear_velocity.x = linear_velocity[0];
		// relation.linear_velocity.y = -linear_velocity[1];
		// relation.linear_velocity.z = -linear_velocity[2];

		relation.relation_flags = (enum xrt_space_relation_flags)(
		    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
		    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT | XRT_SPACE_RELATION_POSITION_TRACKED_BIT |
		    XRT_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT);

		m_relation_history_push(xv_xr50->relation_hist, &relation, timestamp_ns);
	}
	return 0;
}

static void *
xvisio_run_position_and_orientation_thread(void *ptr)
{
	struct xvisio_xr50 *xv_xr50 = (struct xvisio_xr50 *)ptr;
	XV_XR50_DEBUG(xv_xr50, "Creating XVisio SeerSense XR50 device.");

	// FISHEYE
    // if (xv_xr50->xr50->fisheyeCameras()) {
	// 	XV_XR50_DEBUG(xv_xr50, "Enabling fisheye streams.");
    //     xv_xr50->fisheye_left_id = xv_xr50->xr50->fisheyeCameras()->registerCallback(fisheye_left_callback);
    //     xv_xr50->fisheye_right_id = xv_xr50->xr50->fisheyeCameras()->registerCallback(fisheye_right_callback);

    //     xv_xr50->xr50->fisheyeCameras()->start();
    //     xv_xr50->xr50->fisheyeCameras()->start();
    // }

    // SLAM
    if (xv_xr50->xr50->slam()) {
		XV_XR50_DEBUG(xv_xr50, "Enabling slam in mixed mode.");
        xv_xr50->xr50->slam()->start(xv::Slam::Mode::Mixed);
    }

	// to figure out the correct rotations an ancient technique of brute-force is applied
	const struct xrt_vec3 z_axis = XRT_VEC3_UNIT_Z;
	struct xrt_quat rotation_quat = XRT_QUAT_IDENTITY;
	math_quat_from_angle_vector(DEG_TO_RAD(180), &z_axis, &rotation_quat);

	os_thread_helper_lock(&xv_xr50->oth);

	while (os_thread_helper_is_running_locked(&xv_xr50->oth)) {

		os_thread_helper_unlock(&xv_xr50->oth);

		int ret = push_position_and_orientation(xv_xr50, &rotation_quat);
		if (ret < 0) {
			return NULL;
		}
	}

	return NULL;
}

static void
set_xvisio_log_level() {
	switch(debug_get_log_option_xvisio_xr50_log()) {
		case U_LOGGING_DEBUG:
    		xv::setLogLevel(xv::LogLevel::debug);
			break;
		case U_LOGGING_INFO:
    		xv::setLogLevel(xv::LogLevel::info);
			break;
		case U_LOGGING_WARN:
    		xv::setLogLevel(xv::LogLevel::warn);
			break;
		case U_LOGGING_ERROR:
    		xv::setLogLevel(xv::LogLevel::err);
			break;
		case U_LOGGING_TRACE:
		case U_LOGGING_RAW:
		default:
    		xv::setLogLevel(xv::LogLevel::debug);
	}
}

static void
xvisio_xr50_update_inputs(struct xrt_device *xdev)
{
	// Empty
}

static void
xvisio_xr50_get_tracked_pose(struct xrt_device *xdev,
                             enum xrt_input_name name,
                             uint64_t at_timestamp_ns,
                             struct xrt_space_relation *out_relation)
{
	struct xvisio_xr50 *xr50 = xv_xr50(xdev);

	if (name != XRT_INPUT_GENERIC_TRACKER_POSE) {
		XV_XR50_ERROR(xr50, "Unknown input name.");
		return;
	}

	m_relation_history_get(xr50->relation_hist, at_timestamp_ns, out_relation);
}

static void
xvisio_xr50_get_view_poses(struct xrt_device *xdev,
                           const struct xrt_vec3 *default_eye_relation,
                           uint64_t at_timestamp_ns,
                           uint32_t view_count,
                           struct xrt_space_relation *out_head_relation,
                           struct xrt_fov *out_fovs,
                           struct xrt_pose *out_poses)
{
	// Empty
}

struct xrt_device *
xvisio_xr50_create(void)
{
	enum u_device_alloc_flags flags = U_DEVICE_ALLOC_TRACKING_NONE;
	struct xvisio_xr50 *xvisio_xr50 = U_DEVICE_ALLOCATE(struct xvisio_xr50, flags, 1, 0);

    std::map<std::string,std::shared_ptr<xv::Device>> devices = xv::getDevices(10., "", nullptr, xv::SlamStartMode::Normal);
    if (devices.empty()) {
        XV_XR50_ERROR(xvisio_xr50, "Timeout for device detection.");
        xvisio_xr50_destroy(&xvisio_xr50->base);
        return NULL;
    } else {
        XV_XR50_DEBUG(xvisio_xr50, "Found %lu device(s).", devices.size());
    }
    // use the first device in the list
	xvisio_xr50->xr50 = devices.begin()->second;

	m_relation_history_create(&xvisio_xr50->relation_hist);

	xvisio_xr50->log_level = debug_get_log_option_xvisio_xr50_log();
	set_xvisio_log_level();

	xvisio_xr50->base.update_inputs = xvisio_xr50_update_inputs;
	xvisio_xr50->base.get_tracked_pose = xvisio_xr50_get_tracked_pose;
	xvisio_xr50->base.get_view_poses = xvisio_xr50_get_view_poses;
	xvisio_xr50->base.destroy = xvisio_xr50_destroy;
	xvisio_xr50->base.name = XRT_DEVICE_XVISIO;
	xvisio_xr50->base.tracking_origin->type = XRT_TRACKING_TYPE_OTHER;
	xvisio_xr50->base.tracking_origin->offset = (struct xrt_pose)XRT_POSE_IDENTITY;

	snprintf(xvisio_xr50->base.str, XRT_DEVICE_NAME_LEN, "XVisio SeerSense XR50");
	snprintf(xvisio_xr50->base.serial, XRT_DEVICE_NAME_LEN, "XVisio SeerSense XR50");

	xvisio_xr50->base.inputs[0].name = XRT_INPUT_GENERIC_TRACKER_POSE;

	xvisio_xr50->base.device_type = XRT_DEVICE_TYPE_GENERIC_TRACKER;
	xvisio_xr50->base.orientation_tracking_supported = true;
	xvisio_xr50->base.position_tracking_supported = true;

	int ret = 0;

	// Thread and other state.
	ret = os_thread_helper_init(&xvisio_xr50->oth);
	if (ret != 0) {
		XV_XR50_ERROR(xvisio_xr50, "Failed to init threading!");
		xvisio_xr50_destroy(&xvisio_xr50->base);
		return NULL;
	}

	ret = os_thread_helper_start(&xvisio_xr50->oth, xvisio_run_position_and_orientation_thread, xvisio_xr50);
	if (ret != 0) {
		XV_XR50_ERROR(xvisio_xr50, "Failed to start thread!");
		xvisio_xr50_destroy(&xvisio_xr50->base);
		return NULL;
	}

	return &xvisio_xr50->base;
}
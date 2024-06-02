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
#include "xrt/xrt_frameserver.h"
#include "math/m_relation_history.h"
#include "math/m_mathinclude.h"
#include "math/m_api.h"

#include "tracking/t_tracking.h"

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
#include "xv_util.h"

#include "xv-sdk.h"

#include <thread>
#include <stdio.h>
#include <iostream>
#include <chrono>

DEBUG_GET_ONCE_LOG_OPTION(xvisio_xr50_log, "XVISIO_XR50_LOG", U_LOGGING_WARN)
DEBUG_GET_ONCE_LOG_OPTION(xvisio_xr50_fs_log, "XVISIO_XR50_FS_LOG", U_LOGGING_WARN)

#define XV_XR50_TRACE(xvisio, ...) U_LOG_XDEV_IFL_T(&xvisio->base, xvisio->log_level, __VA_ARGS__)
#define XV_XR50_DEBUG(xvisio, ...) U_LOG_XDEV_IFL_D(&xvisio->base, xvisio->log_level, __VA_ARGS__)
#define XV_XR50_ERROR(xvisio, ...) U_LOG_XDEV_IFL_E(&xvisio->base, xvisio->log_level, __VA_ARGS__)

#define DEG_TO_RAD(D) ((D)*M_PI / 180.)

std::ostream& operator<<(std::ostream& o, const xv::Calibration& calibration)
{
    o << "translation\n";
    o << "x: " << calibration.pose.x() << std::endl;
    o << "y: " << calibration.pose.y() << std::endl;
    o << "z: " << calibration.pose.z() << std::endl;
    // 3x3 rotation matrix row major
    double _1 = calibration.pose.rotation()[0];
    double _2 = calibration.pose.rotation()[1];
    double _3 = calibration.pose.rotation()[2];
    double _4 = calibration.pose.rotation()[3];
    double _5 = calibration.pose.rotation()[4];
    double _6 = calibration.pose.rotation()[5];
    double _7 = calibration.pose.rotation()[6];
    double _8 = calibration.pose.rotation()[7];
    double _9 = calibration.pose.rotation()[8];
    o << "rotation\n";
    o << "[\t" << _1 << "\t" << _2 << "\t" << _3 << std::endl;
    o << "\t" << _4 << "\t" << _5 << "\t" << _6 << std::endl;
    o << "\t" << _7 << "\t" << _8 << "\t" << _9 << "]" << std::endl;

	// sdk doesn't yet support this struct's functions
    // xv::CameraModel camera_model = *calibration.camerasModel[0];

    return o;
}

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

struct xvisio_frameserver {
	struct xrt_fs base;
	struct xrt_frame_node node;

	std::shared_ptr<const xv::FisheyeImages> fisheye_stereo;

	int fisheye_callback_id;

	u_logging_level log_level;

	uint32_t width;
	uint32_t height;
	xrt_format format;

	// left and right sinks
	xrt_frame_sink *sink[2];

	std::shared_ptr<xv::Device> xr50;
};

static inline struct xvisio_xr50 *
xv_xr50(struct xrt_device *xdev)
{
	return (struct xvisio_xr50 *)xdev;
}

static void
xvisio_xr50_destroy(struct xrt_device *xdev)
{
	struct xvisio_xr50 *xvisio_xr50 = xv_xr50(xdev);
	XV_XR50_DEBUG(xvisio_xr50, "Destroying XVisio XR50.");

	// 1. lock mutex and shutdown thread
	os_thread_helper_destroy(&xvisio_xr50->oth);

	// 2. shutdown device
	xvisio_xr50->xr50->slam()->stop();

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

// the latest sdk: version 3.2.0-20230907 doesnt support reading the intrinsics

// left intrinsics (example)
// SEUCM0: {w=640, h=400, fx=275.567, fy=275.567, u0=312.998, v0=215.125, eu=314.234ev=215.115alpha=0.596733beta=1.12012}

// right intrinsics (example)
// SEUCM0: {w=640, h=400, fx=277.432, fy=277.432, u0=315.526, v0=205.492, eu=316.662ev=205.94alpha=0.605949beta=1.09396}

static bool
xvisio_get_stereo_camera_calibration(struct xvisio_frameserver *frameserver, struct t_stereo_camera_calibration **c_ptr) {
	struct xr50_camera_calibration_stereo *camera_calibration_stereo_eucm = (struct xr50_camera_calibration_stereo *)malloc(sizeof(struct xr50_camera_calibration_stereo));

	// left fisheye calibration
	struct xv::Calibration lfc = frameserver->xr50->fisheyeCameras()->calibration()[0];
	// right fisheye calibration
	struct xv::Calibration rfc = frameserver->xr50->fisheyeCameras()->calibration()[1];
	
	// left camera
	struct xrt_matrix_4x4 left_camera_from_imu = {
		lfc.pose.rotation()[0],	lfc.pose.rotation()[1],	lfc.pose.rotation()[2],	lfc.pose.x(),
		lfc.pose.rotation()[3],	lfc.pose.rotation()[4],	lfc.pose.rotation()[5],	lfc.pose.y(),
		lfc.pose.rotation()[6],	lfc.pose.rotation()[7],	lfc.pose.rotation()[8],	lfc.pose.z(),
		0,						0,						0,						1,
	};

	camera_calibration_stereo_eucm->cameras[0].camera_from_imu = left_camera_from_imu;
	/* Monado / Eigen expect column major 4x4 isometry, so transpose */
	math_matrix_4x4_transpose(&camera_calibration_stereo_eucm->cameras[0].camera_from_imu, &camera_calibration_stereo_eucm->cameras[0].camera_from_imu);
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.image_size_pixels.w = 640;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.image_size_pixels.h = 400;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.projection.cx = 312.998;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.projection.cy = 215.125;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.projection.fx = 275.567;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.projection.fy = 275.567;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.distortion.alpha = 0.596733;
	camera_calibration_stereo_eucm->cameras[0].camera_calibration.distortion.beta = 1.12012;

	// right camera
	struct xrt_matrix_4x4 right_camera_from_imu = {
		rfc.pose.rotation()[0],	rfc.pose.rotation()[1],	rfc.pose.rotation()[2],	rfc.pose.x(),
		rfc.pose.rotation()[3],	rfc.pose.rotation()[4],	rfc.pose.rotation()[5],	rfc.pose.y(),
		rfc.pose.rotation()[6],	rfc.pose.rotation()[7],	rfc.pose.rotation()[8],	rfc.pose.z(),
		0,						0,						0,						1,
	};
	camera_calibration_stereo_eucm->cameras[1].camera_from_imu = right_camera_from_imu;
	/* Monado / Eigen expect column major 4x4 isometry, so transpose */
	math_matrix_4x4_transpose(&camera_calibration_stereo_eucm->cameras[1].camera_from_imu, &camera_calibration_stereo_eucm->cameras[1].camera_from_imu);
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.image_size_pixels.w = 640;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.image_size_pixels.h = 400;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.projection.cx = 315.526;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.projection.cy = 205.492;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.projection.fx = 277.432;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.projection.fy = 277.432;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.distortion.alpha = 0.605949;
	camera_calibration_stereo_eucm->cameras[1].camera_calibration.distortion.beta = 1.09396;

	// convert from devices' EUCM camera model to the KB4 camera model
	struct t_stereo_camera_calibration *calib_kb4 = xvisio_xr50_create_stereo_camera_calib_rotated(camera_calibration_stereo_eucm);
	
	// To properly handle ref counting.
	t_stereo_camera_calibration_reference(c_ptr, calib_kb4);
	t_stereo_camera_calibration_reference(&calib_kb4, NULL);

	return true;
}

static inline struct xvisio_frameserver *
xvisio_frameserver(struct xrt_fs *xfs)
{
	return (struct xvisio_frameserver *)xfs;
}

void
frame_destroy(struct xrt_frame *xf)
{
	delete xf;
}

static bool
xvisio_frameserver_slam_stream_start(struct xrt_fs *xfs, struct xrt_slam_sinks *sinks)
{
	struct xvisio_frameserver *frameserver = xvisio_frameserver(xfs);

	frameserver->sink[0] = sinks->cams[0]; // left
	frameserver->sink[1] = sinks->cams[1]; // right

	if (frameserver->xr50->fisheyeCameras()) {
		frameserver->fisheye_callback_id = frameserver->xr50->fisheyeCameras()->registerCallback( [frameserver](xv::FisheyeImages const & stereo){
			frameserver->fisheye_stereo = std::make_shared<const xv::FisheyeImages>(stereo);

			auto now = std::chrono::system_clock::now();
			uint64_t nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

			// left
			struct xrt_frame *frame_left = (struct xrt_frame *)malloc(sizeof(struct xrt_frame));
			frame_left->width = frameserver->fisheye_stereo->images[0].width;
			frame_left->height = frameserver->fisheye_stereo->images[0].height;
			frame_left->format = XRT_FORMAT_R8;
			frame_left->timestamp = nanoseconds;
			frame_left->reference.count = 1;
			frame_left->destroy = frame_destroy;
			int frame_size_left = frame_left->width * frame_left->height;
			const unsigned char *stereo_image_data_left = frameserver->fisheye_stereo->images[0].data.get();
			// allocate
			frame_left->data = (uint8_t *)malloc(sizeof(uint8_t) * frame_size_left);
			// copy
			memcpy(frame_left->data, stereo_image_data_left, frame_size_left);
			// stride, size
			u_format_size_for_dimensions(frame_left->format, frame_left->width, frame_left->height, &frame_left->stride, &frame_left->size);

			// right
			struct xrt_frame *frame_right = (struct xrt_frame *)malloc(sizeof(struct xrt_frame));
			frame_right->width = frameserver->fisheye_stereo->images[1].width;
			frame_right->height = frameserver->fisheye_stereo->images[1].height;
			frame_right->format = XRT_FORMAT_R8;
			frame_right->timestamp = nanoseconds;
			frame_right->reference.count = 1;
			frame_right->destroy = frame_destroy;
			int frame_size_right = frame_right->width * frame_right->height;
			const unsigned char *stereo_image_data_right = frameserver->fisheye_stereo->images[1].data.get();
			// allocate
			frame_right->data = (uint8_t *)malloc(sizeof(uint8_t) * frame_size_right);
			// copy
			memcpy(frame_right->data, stereo_image_data_right, frame_size_right);
			// stride, size
			u_format_size_for_dimensions(frame_right->format, frame_right->width, frame_right->height, &frame_right->stride, &frame_right->size);
			
			// push frames
			xrt_sink_push_frame(frameserver->sink[0], frame_left);
			xrt_sink_push_frame(frameserver->sink[1], frame_right);

			xrt_frame_reference(&frame_left, NULL);
			xrt_frame_reference(&frame_right, NULL);

			free(frame_left);
			free(frame_right);
        });

        frameserver->xr50->fisheyeCameras()->start();
    }

	return true;
}

static bool
xvisio_frameserver_stream_stop(struct xrt_fs *xfs)
{
	struct xvisio_frameserver *frameserver = xvisio_frameserver(xfs);

	frameserver->xr50->fisheyeCameras()->unregisterCallback(frameserver->fisheye_callback_id);
	frameserver->xr50->fisheyeCameras()->stop();

	return true;
}

static bool
xvisio_frameserver_is_running(struct xrt_fs *xfs)
{
	return true;
}

static bool
xvisio_frameserver_destroy(struct xvisio_frameserver *frameserver)
{
	frameserver->xr50->fisheyeCameras()->unregisterCallback(frameserver->fisheye_callback_id);
	frameserver->xr50->fisheyeCameras()->stop();

	delete &frameserver->xr50;

	free(frameserver);

	return true;
}

static void
xvisio_frameserver_node_break_apart(struct xrt_frame_node *node)
{
	struct xvisio_frameserver *frameserver = container_of(node, struct xvisio_frameserver, node);

	xvisio_frameserver_stream_stop(&frameserver->base);
}

static void
xvisio_frameserver_node_destroy(struct xrt_frame_node *node)
{
	struct xvisio_frameserver *frameserver = container_of(node, struct xvisio_frameserver, node);

	xvisio_frameserver_destroy(frameserver);
}

struct xrt_fs *
xvisio_frameserver_create(struct xrt_frame_context *xfctx)
{
	struct xvisio_frameserver *frameserver = U_TYPED_CALLOC(struct xvisio_frameserver);

	std::map<std::string,std::shared_ptr<xv::Device>> devices = xv::getDevices(10., "", nullptr, xv::SlamStartMode::Normal);

	// xv::setLogLevel(xv::LogLevel::debug);

	// note that this driver doesn't support all functionality
	frameserver->xr50 = devices.begin()->second;

	frameserver->base.slam_stream_start = xvisio_frameserver_slam_stream_start;
	frameserver->base.stream_stop = xvisio_frameserver_stream_stop;
	frameserver->base.is_running = xvisio_frameserver_is_running;

	frameserver->node.break_apart = xvisio_frameserver_node_break_apart;
	frameserver->node.destroy = xvisio_frameserver_node_destroy;

	frameserver->log_level = debug_get_log_option_xvisio_xr50_log();

	xrt_frame_context_add(xfctx, &frameserver->node);

	return &frameserver->base;
}

bool
xvisio_frameserver_get_stereo_calibration(struct xrt_fs *xfs, struct t_stereo_camera_calibration **c_ptr)
{
	// holds the xr50 device
	struct xvisio_frameserver *frameserver = xvisio_frameserver(xfs);

	// uses the xr50 device reference to call calibration retrieval functions
	return xvisio_get_stereo_camera_calibration(frameserver, c_ptr);
}
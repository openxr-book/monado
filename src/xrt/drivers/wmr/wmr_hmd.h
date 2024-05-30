// Copyright 2018, Philipp Zabel.
// Copyright 2020-2021, N Madsen.
// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to the WMR HMD driver code.
 * @author Philipp Zabel <philipp.zabel@gmail.com>
 * @author nima01 <nima_zero_one@protonmail.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Nova King <technobaboo@proton.me>
 * @ingroup drv_wmr
 */

#pragma once

#include "tracking/t_tracking.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_frame.h"
#include "xrt/xrt_prober.h"
#include "os/os_threading.h"
#include "math/m_imu_3dof.h"
#include "util/u_logging.h"
#include "util/u_distortion_mesh.h"
#include "util/u_var.h"

#include "wmr_protocol.h"
#include "wmr_config.h"
#include "wmr_camera.h"
#include "wmr_common.h"
#include "wmr_hmd_controller.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Support 2 controllers on HP Reverb G2 */
#define WMR_MAX_CONTROLLERS 2

struct wmr_hmd;

struct wmr_headset_descriptor
{
	enum wmr_headset_type hmd_type;

	//! String by which we recognise the device
	const char *dev_id_str;
	int vid;
	int pid;
	bool is_well_supported;
	//! Friendly ID string for debug
	const char *debug_name;

	int (*init_func)(struct wmr_hmd *wh);
	void (*deinit_func)(struct wmr_hmd *wh);
	void (*screen_enable_func)(struct wmr_hmd *wh, bool enable);
};

struct wmr_hmd_distortion_params
{
	//! Inverse affine transform to move from (undistorted) pixels
	//! to image plane / normalised image coordinates
	struct xrt_matrix_3x3 inv_affine_xform;

	//! tan(angle) FoV min/max for X and Y in the input texture
	struct xrt_vec2 tex_x_range;
	struct xrt_vec2 tex_y_range;
};

/*!
 * @implements xrt_device
 */
struct wmr_hmd
{
	struct xrt_device base;

	const struct wmr_headset_descriptor *hmd_desc;

	//! firmware configuration block, with device names etc
	struct wmr_config_header config_hdr;

	//! Config data parsed from the firmware JSON
	struct wmr_hmd_config config;

	//! Packet reading thread.
	struct os_thread_helper oth;

	enum u_logging_level log_level;

	int32_t left_view_y_offset, right_view_y_offset;

	/*!
	 * This is the Hololens Sensors device, this is where we get all of the
	 * IMU data and read the config from.
	 *
	 * During start it is owned by the thread creating the device, after
	 * init it is owned by the reading thread. Read/write access is
	 * protected by the hid_lock
	 */

	struct os_hid_device *hid_hololens_sensors_dev;
	struct os_mutex hid_lock;

	/*!
	 * This is the vendor specific companion device of the Hololens Sensors.
	 * When activated, it will report the physical IPD adjustment and proximity
	 * sensor status of the headset. It also allows enabling/disabling the HMD
	 * screen on Reverb G1/G2.
	 */
	struct os_hid_device *hid_control_dev;

	//! Current desired HMD screen state.
	bool hmd_screen_enable;
	//! Latest raw IPD value read from the device.
	uint16_t raw_ipd;
	//! Latest proximity sensor value read from the device.
	uint8_t proximity_sensor;

	//! Distortion related parameters
	struct wmr_hmd_distortion_params distortion_params[2];

	struct hololens_sensors_packet packet;

	struct
	{
		//! Protects all members of the `fusion` substruct.
		struct os_mutex mutex;

		//! Main fusion calculator.
		struct m_imu_3dof i3dof;

		//! The last angular velocity from the IMU, for prediction.
		struct xrt_vec3 last_angular_velocity;

		//! When did we get the last IMU sample, in CPU time.
		uint64_t last_imu_timestamp_ns;
	} fusion;

	//! Fields related to camera-based tracking (SLAM and hand tracking)
	struct
	{
		//! Source of video/IMU data for tracking
		struct xrt_fs *source;

		//! Context for @ref source
		struct xrt_frame_context xfctx;

		//! SLAM tracker.
		//! @todo Right now, we are not consistent in how we interface with
		//! trackers. In particular, we have a @ref xrt_tracked_slam field but not
		//! an equivalent for hand tracking.
		struct xrt_tracked_slam *slam;

		//! Calibration data for SLAM
		struct t_slam_calibration slam_calib;

		//! Set at start. Whether the SLAM tracker was initialized.
		bool slam_enabled;

		//! Set at start. Whether the hand tracker was initialized.
		bool hand_enabled;

		//! SLAM systems track the IMU pose, enabling this corrects it to middle of the eyes
		bool imu2me;
	} tracking;

	//! Whether to track the HMD with 6dof SLAM or fallback to the `fusion` 3dof tracker
	bool slam_over_3dof;

	//! Last tracked pose
	struct xrt_pose pose;

	//! Additional offset to apply to `pose`
	struct xrt_pose offset;

	//! Average 4 IMU samples before sending them to the trackers
	bool average_imus;

	/*!
	 * Offset for tracked pose offsets (applies to both fusion and SLAM).
	 * Applied when getting the tracked poses, so is effectively a offset
	 * to increase or decrease prediction.
	 */
	struct u_var_draggable_f32 tracked_offset_ms;

	struct
	{
		struct u_var_button hmd_screen_enable_btn;
		struct u_var_button switch_tracker_btn;
		char hand_status[128];
		char slam_status[128];
	} gui;

	/* Tunnelled controller devices (Reverb G2, Odyssey+) handling */
	struct os_mutex controller_status_lock;
	struct os_cond controller_status_cond;
	bool have_left_controller_status;
	bool have_right_controller_status;

	struct wmr_hmd_controller_connection *controller[WMR_MAX_CONTROLLERS];
};

static inline struct wmr_hmd *
wmr_hmd(struct xrt_device *p)
{
	return (struct wmr_hmd *)p;
}

void
wmr_hmd_create(enum wmr_headset_type hmd_type,
               struct os_hid_device *hid_holo,
               struct os_hid_device *hid_ctrl,
               struct xrt_prober_device *dev_holo,
               enum u_logging_level log_level,
               struct xrt_device **out_hmd,
               struct xrt_device **out_handtracker,
               struct xrt_device **out_left_controller,
               struct xrt_device **out_right_controller);

bool
wmr_hmd_send_controller_packet(struct wmr_hmd *hmd, const uint8_t *buffer, uint32_t buf_size);
int
wmr_hmd_read_sync_from_controller(struct wmr_hmd *hmd, uint8_t *buffer, uint32_t buf_size, int timeout_ms);
const struct wmr_headset_descriptor *
get_wmr_headset_map();
int
get_wmr_headset_map_size();
#ifdef __cplusplus
}
#endif

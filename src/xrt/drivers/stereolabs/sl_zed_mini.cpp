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

#include "os/os_time.h"

#include "math/m_api.h"
#include "math/m_mathinclude.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_time.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_logging.h"
#include "util/u_distortion_mesh.h"

#include "sl_interface.h"

#include "sl/Camera.hpp"

#include <stdio.h>


/*
 *
 * Structs and defines.
 *
 */

/*!
 * A sample HMD device.
 *
 * @implements xrt_device
 */
struct sl_zed_mini
{
	struct xrt_device base;

	struct xrt_pose pose;

	enum u_logging_level log_level;
};


/// Casting helper function
static inline struct sl_zed_mini *
sl_zed_mini(struct xrt_device *xdev)
{
	return (struct sl_zed_mini *)xdev;
}

DEBUG_GET_ONCE_LOG_OPTION(stereolabs_log, "STEREOLABS_LOG", U_LOGGING_WARN)

#define SL_TRACE(sl, ...) U_LOG_XDEV_IFL_T(&sl->base, sl->log_level, __VA_ARGS__)
#define SL_DEBUG(sl, ...) U_LOG_XDEV_IFL_D(&sl->base, sl->log_level, __VA_ARGS__)
#define SL_ERROR(sl, ...) U_LOG_XDEV_IFL_E(&sl->base, sl->log_level, __VA_ARGS__)

static void
sl_zed_mini_destroy(struct xrt_device *xdev)
{
	struct sl_zed_mini *sl_zm = sl_zed_mini(xdev);

	// Remove the variable tracking.
	u_var_remove_root(sl_zm);

	u_device_free(&sl_zm->base);
}

static void
sl_zed_mini_update_inputs(struct xrt_device *xdev)
{
	// Empty, you should put code to update the attached input fields (if any)
}

static void
sl_zed_mini_get_tracked_pose(struct xrt_device *xdev,
                            enum xrt_input_name name,
                            uint64_t at_timestamp_ns,
                            struct xrt_space_relation *out_relation)
{
	struct sl_zed_mini *sl_zm = sl_zed_mini(xdev);

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		SL_ERROR(sl_zm, "unknown input name");
		return;
	}

	// Estimate pose at timestamp at_timestamp_ns!
	math_quat_normalize(&sl_zm->pose.orientation);
	out_relation->pose = sl_zm->pose;
	out_relation->relation_flags = (enum xrt_space_relation_flags)(XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |
	                                                               XRT_SPACE_RELATION_POSITION_VALID_BIT |
	                                                               XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);
}
/*
static void
sl_zed_mini_get_view_poses(struct xrt_device *xdev,
                          const struct xrt_vec3 *default_eye_relation,
                          uint64_t at_timestamp_ns,
                          uint32_t view_count,
                          struct xrt_space_relation *out_head_relation,
                          struct xrt_fov *out_fovs,
                          struct xrt_pose *out_poses)
{
	u_device_get_view_poses(xdev, default_eye_relation, at_timestamp_ns, view_count, out_head_relation, out_fovs,
	                        out_poses);
}
*/
struct xrt_device *
sl_zed_mini_create(void)
{
	/*
	//TODO: create stereolabs specific device
	struct sl_zed_mini *sl_zm = U_DEVICE_ALLOCATE(struct sl_zed_mini, flags, 1, 0);
	sl_zm->log_level = debug_get_log_option_sl_log();

	// This list should be ordered, most preferred first.
	size_t idx = 0;
	sh->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_OPAQUE;
	sh->base.hmd->blend_mode_count = idx;

	sh->base.update_inputs = sl_zed_mini_update_inputs;
	sh->base.get_tracked_pose = sl_zed_mini_get_tracked_pose;
	sh->base.get_view_poses = sl_zed_mini_get_view_poses;
	sh->base.destroy = sl_zed_mini_destroy;

	sh->pose = (struct xrt_pose)XRT_POSE_IDENTITY;
	

	// Print name.
	snprintf(sh->base.str, XRT_DEVICE_NAME_LEN, "Sample HMD");
	snprintf(sh->base.serial, XRT_DEVICE_NAME_LEN, "Sample HMD S/N");

	// Setup input.
	sh->base.name = XRT_DEVICE_GENERIC_HMD;
	sh->base.device_type = XRT_DEVICE_TYPE_HMD;
	sh->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;
	sh->base.orientation_tracking_supported = true;
	sh->base.position_tracking_supported = false;

	// Set up display details
	// refresh rate
	sh->base.hmd->screens[0].nominal_frame_interval_ns = time_s_to_ns(1.0f / 90.0f);

	const double hFOV = 90 * (M_PI / 180.0);
	const double vFOV = 96.73 * (M_PI / 180.0);
	// center of projection
	const double hCOP = 0.529;
	const double vCOP = 0.5;

	const int panel_w = 1080;
	const int panel_h = 1200;

	// Single "screen" (always the case)
	sh->base.hmd->screens[0].w_pixels = panel_w * 2;
	sh->base.hmd->screens[0].h_pixels = panel_h;

	// Left, Right
	for (uint8_t eye = 0; eye < 2; ++eye) {
		sh->base.hmd->views[eye].display.w_pixels = panel_w;
		sh->base.hmd->views[eye].display.h_pixels = panel_h;
		sh->base.hmd->views[eye].viewport.y_pixels = 0;
		sh->base.hmd->views[eye].viewport.w_pixels = panel_w;
		sh->base.hmd->views[eye].viewport.h_pixels = panel_h;
		// if rotation is not identity, the dimensions can get more complex.
		sh->base.hmd->views[eye].rot = u_device_rotation_ident;
	}
	// left eye starts at x=0, right eye starts at x=panel_width
	sh->base.hmd->views[0].viewport.x_pixels = 0;
	sh->base.hmd->views[1].viewport.x_pixels = panel_w;

	// Distortion information, fills in xdev->compute_distortion().
	u_distortion_mesh_set_none(&sh->base);

	// Setup variable tracker: Optional but useful for debugging
	u_var_add_root(sh, "Sample HMD", true);
	u_var_add_pose(sh, &sh->pose, "pose");
	u_var_add_log_level(sh, &sh->log_level, "log_level");


	return &sh->base;
	*/
}

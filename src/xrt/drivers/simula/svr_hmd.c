// Copyright 2020, Collabora, Ltd.
// Copyright 2020, Nova King.
// Copyright 2020, Moses Turner.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  North Star HMD code.
 * @author Nova King <technobaboo@gmail.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Moses Turner <mosesturner@protonmail.com>
 * @author Nico Zobernig <nico.zobernig@gmail.com>
 * @ingroup drv_ns
 */

#include "math/m_mathinclude.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "math/m_vec2.h"
#include "os/os_time.h"

#include "svr_hmd.h"

#include "util/u_var.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_time.h"

#include "math/m_space.h"

DEBUG_GET_ONCE_LOG_OPTION(svr_log, "SIMULA_LOG", U_LOGGING_INFO)


static void
svr_hmd_destroy(struct xrt_device *xdev)
{
	struct svr_hmd *ns = svr_hmd(xdev);

	// Remove the variable tracking.
	u_var_remove_root(ns);

	u_device_free(&ns->base);
}
//
static void
svr_hmd_update_inputs(struct xrt_device *xdev)
{}

static void
svr_hmd_get_tracked_pose(struct xrt_device *xdev,
                         enum xrt_input_name name,
                         uint64_t at_timestamp_ns,
                         struct xrt_space_relation *out_relation)
{
	struct svr_hmd *ns = svr_hmd(xdev);

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		SVR_ERROR(ns, "unknown input name");
		return;
	}


	out_relation->angular_velocity = (struct xrt_vec3)XRT_VEC3_ZERO;
	out_relation->linear_velocity = (struct xrt_vec3)XRT_VEC3_ZERO;
	out_relation->pose =
	    (struct xrt_pose)XRT_POSE_IDENTITY; // This is so that tracking overrides/multi driver just transforms us by
	                                        // the tracker + offset from the tracker.
	out_relation->relation_flags = XRT_SPACE_RELATION_BITMASK_ALL;
}

#define DEG_TO_RAD(DEG) (DEG * M_PI / 180.)

static void
svr_hmd_get_view_poses(struct xrt_device *xdev,
                       const struct xrt_vec3 *default_eye_relation,
                       uint64_t at_timestamp_ns,
                       uint32_t view_count,
                       struct xrt_space_relation *out_head_relation,
                       struct xrt_fov *out_fovs,
                       struct xrt_pose *out_poses)
{
	//!@todo: default_eye_relation inherits from the env var OXR_DEBUG_IPD_MM / oxr_session.c
	// probably needs a lot more attention

	u_device_get_view_poses(xdev, default_eye_relation, at_timestamp_ns, view_count, out_head_relation, out_fovs,
	                        out_poses);



	//!@todo you may need to invert this - I can't test locally
	float turn_vals[2] = {5.0, -5.0};
	for (uint32_t i = 0; i < view_count && i < 2; i++) {
		struct xrt_vec3 y_up = (struct xrt_vec3)XRT_VEC3_UNIT_Y;
		math_quat_from_angle_vector(DEG_TO_RAD(turn_vals[i]), &y_up, &out_poses[i].orientation);
	}
}

/*
 *
 * Create function.
 *
 */

struct xrt_device *
svr_hmd_create()
{
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct svr_hmd *svr = U_DEVICE_ALLOCATE(struct svr_hmd, flags, 1, 0);
	svr->log_level = debug_get_log_option_svr_log();



	svr->base.update_inputs = svr_hmd_update_inputs;
	svr->base.get_tracked_pose = svr_hmd_get_tracked_pose;
	svr->base.get_view_poses = svr_hmd_get_view_poses;
	svr->base.destroy = svr_hmd_destroy;
	svr->base.name = XRT_DEVICE_GENERIC_HMD;

	// Lies!
	svr->base.orientation_tracking_supported = true;
	svr->base.position_tracking_supported = true;

	// Truth!
	svr->base.device_type = XRT_DEVICE_TYPE_HMD;


	// Print name.
	snprintf(svr->base.str, XRT_DEVICE_NAME_LEN, "SimulaVR HMD");
	snprintf(svr->base.serial, XRT_DEVICE_NAME_LEN, "0001");
	// Setup input.
	svr->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;

	struct u_extents_2d exts;

	// one screen is 2448px wide, but there are two of them.
	exts.w_pixels = 2448 * 2;
	// Both screens are 2448px tall
	exts.h_pixels = 2448;

	u_extents_2d_split_side_by_side(&svr->base, &exts);

	for (int view = 0; view < 2; view++) {
		//!@todo hardcoded, should result in good fov and not too much stretching but should be adjusted to real
		//! hardware
		svr->base.hmd->distortion.fov[view].angle_left = -HALF_FOV;
		svr->base.hmd->distortion.fov[view].angle_right = HALF_FOV;
		svr->base.hmd->distortion.fov[view].angle_up = HALF_FOV;
		svr->base.hmd->distortion.fov[view].angle_down = -HALF_FOV;
	}

	u_distortion_mesh_set_none(&svr->base);

	// Setup variable tracker.
	u_var_add_root(svr, "Simula HMD", true);
	svr->base.orientation_tracking_supported = true;
	svr->base.device_type = XRT_DEVICE_TYPE_HMD;

	size_t idx = 0;

	//!@todo these should be true for the final product iirc but possibly not for the demo unit
	svr->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_ADDITIVE;
	svr->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_OPAQUE;
	svr->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_ALPHA_BLEND;

	svr->base.hmd->blend_mode_count = idx;

	uint64_t start;
	uint64_t end;

	start = os_monotonic_get_ns();
	u_distortion_mesh_fill_in_compute(&svr->base);
	end = os_monotonic_get_ns();

	float diff = (end - start);
	diff /= U_TIME_1MS_IN_NS;

	SVR_DEBUG(svr, "Filling mesh took %f ms", diff);


	return &svr->base;
}

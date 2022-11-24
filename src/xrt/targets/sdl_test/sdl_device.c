// Copyright 2020-2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Shared default implementation of the device with compositor.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */

#include "sdl_internal.h"

#include "util/u_device.h"
#include "util/u_distortion_mesh.h"

static void
sdl_hmd_update_inputs(struct xrt_device *xdev)
{
	// Empty, you should put code to update the attached inputs fields.
}

static void
sdl_hmd_get_tracked_pose(struct xrt_device *xdev,
                         enum xrt_input_name name,
                         uint64_t at_timestamp_ns,
                         struct xrt_space_relation *out_relation)
{
	struct sdl_program *sp = from_xdev(xdev);

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		U_LOG_E("Unknown input name");
		return;
	}

	struct xrt_space_relation relation = XRT_SPACE_RELATION_ZERO;

	relation.pose = sp->state.head.pose;
	relation.relation_flags =                       //
	    XRT_SPACE_RELATION_POSITION_TRACKED_BIT |   //
	    XRT_SPACE_RELATION_POSITION_VALID_BIT |     //
	    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |  //
	    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT; //

	*out_relation = relation;
}

static void
sdl_hmd_get_view_poses(struct xrt_device *xdev,
                       const struct xrt_vec3 *default_eye_relation,
                       uint64_t at_timestamp_ns,
                       uint32_t view_count,
                       struct xrt_space_relation *out_head_relation,
                       struct xrt_fov *out_fovs,
                       struct xrt_pose *out_poses)
{
	u_device_get_view_poses(  //
	    xdev,                 //
	    default_eye_relation, //
	    at_timestamp_ns,      //
	    view_count,           //
	    out_head_relation,    //
	    out_fovs,             //
	    out_poses);           //

	struct sdl_program *sp = from_xdev(xdev);

	out_poses->position.x = -sp->state.position_estimate.x;
	out_poses->position.y = sp->state.position_estimate.y;
	out_poses->position.z = sp->state.position_estimate.z;

	float screen_width = 0.34544f;
	float screen_height = 0.19431f;
	float screen_half_width = screen_width / 2.0f;
	float distance =
	    sp->state.position_estimate.z >= 0.0f ? sp->state.position_estimate.z : -sp->state.position_estimate.z;

	float left = -(screen_half_width - sp->state.position_estimate.x) / distance;
	float right = (screen_half_width + sp->state.position_estimate.x) / distance;
	float up = -(sp->state.position_estimate.y) / distance;
	float down = -(screen_height + sp->state.position_estimate.y) / distance;

	out_fovs->angle_down = atan(down);
	out_fovs->angle_left = atan(left);
	out_fovs->angle_right = atan(right);
	out_fovs->angle_up = atan(up);
}

static void
sdl_hmd_destroy(struct xrt_device *xdev)
{
	struct sdl_program *sp = from_xdev(xdev);

	if (xdev->hmd->distortion.mesh.vertices) {
		free(xdev->hmd->distortion.mesh.vertices);
		xdev->hmd->distortion.mesh.vertices = NULL;
	}

	if (xdev->hmd->distortion.mesh.indices) {
		free(xdev->hmd->distortion.mesh.indices);
		xdev->hmd->distortion.mesh.indices = NULL;
	}

	(void)sp; // We are apart of sdl_program, do not free.
}


/*
 *
 * 'Exported' functions.
 *
 */

void
sdl_device_init(struct sdl_program *sp)
{
	struct xrt_device *xdev = &sp->xdev_base;

	// Setup pointers.
	xdev->inputs = sp->inputs;
	xdev->input_count = ARRAY_SIZE(sp->inputs);
	xdev->tracking_origin = &sp->origin;
	xdev->hmd = &sp->hmd;

	// Name and type.
	xdev->name = XRT_DEVICE_GENERIC_HMD;
	xdev->device_type = XRT_DEVICE_TYPE_HMD;

	// Print name.
	snprintf(xdev->str, XRT_DEVICE_NAME_LEN, "SDL HMD");
	snprintf(xdev->serial, XRT_DEVICE_NAME_LEN, "SDL HMD");

	// Input info.
	xdev->inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;
	xdev->inputs[0].active = true;

	// Function pointers.
	xdev->update_inputs = sdl_hmd_update_inputs;
	xdev->get_tracked_pose = sdl_hmd_get_tracked_pose;
	xdev->get_view_poses = sdl_hmd_get_view_poses;
	xdev->destroy = sdl_hmd_destroy;

	// Minimum needed stuff.
	struct u_device_simple_info info;
	info.display.w_pixels = 1280;
	info.display.h_pixels = 720;
	info.display.w_meters = 0.13f;
	info.display.h_meters = 0.07f;
	info.lens_horizontal_separation_meters = 0.13f / 2.0f;
	info.lens_vertical_position_meters = 0.07f / 2.0f;
	info.fov[0] = 85.0f * ((float)(M_PI) / 180.0f);
	info.fov[1] = 85.0f * ((float)(M_PI) / 180.0f);

	if (!u_device_setup_split_side_by_side(xdev, &info)) {
		U_LOG_E("Failed to setup basic device info");
		return;
	}

	// Refresh rate.
	xdev->hmd->screens[0].nominal_frame_interval_ns = time_s_to_ns(1.0f / 60.0f);

	// Blend mode(s), setup after u_device_setup_split_side_by_side.
	xdev->hmd->blend_modes[0] = XRT_BLEND_MODE_OPAQUE;
	xdev->hmd->blend_mode_count = 1;

	// Distortion information, fills in xdev->compute_distortion().
	u_distortion_mesh_set_none(xdev);

	// Tracking origin.
	xdev->tracking_origin->offset = (struct xrt_pose)XRT_POSE_IDENTITY;
	xdev->tracking_origin->type = XRT_TRACKING_TYPE_OTHER;
	snprintf(xdev->tracking_origin->name, XRT_TRACKING_NAME_LEN, "SDL Tracking");
}

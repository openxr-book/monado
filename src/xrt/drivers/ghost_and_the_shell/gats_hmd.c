#include "math/m_mathinclude.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "os/os_time.h"

#include "gats_hmd.h"

#include "util/u_var.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_time.h"

#include "math/m_space.h"

DEBUG_GET_ONCE_LOG_OPTION(gats_log, "GATS_LOG", U_LOGGING_INFO)

/*
 *
 * Printing functions.
 *
 */

#define GATS_TRACE(d, ...) U_LOG_XDEV_IFL_T(&d->base, d->log_level, __VA_ARGS__)
#define GATS_DEBUG(d, ...) U_LOG_XDEV_IFL_D(&d->base, d->log_level, __VA_ARGS__)
#define GATS_INFO(d, ...) U_LOG_XDEV_IFL_I(&d->base, d->log_level, __VA_ARGS__)
#define GATS_WARN(d, ...) U_LOG_XDEV_IFL_W(&d->base, d->log_level, __VA_ARGS__)
#define GATS_ERROR(d, ...) U_LOG_XDEV_IFL_E(&d->base, d->log_level, __VA_ARGS__)

/*
 *
 * Common functions
 *
 */

static void
gats_hmd_destroy(struct xrt_device *xdev)
{
	struct gats_hmd *gats = gats_hmd(xdev);
	u_device_free(&gats->base);
}

static void
gats_hmd_update_inputs(struct xrt_device *xdev)
{
	//struct gats_hmd *gats = gats_hmd(xdev);
}

static void
gats_hmd_get_tracked_pose(struct xrt_device *xdev,
						enum xrt_input_name name,
                        uint64_t at_timestamp_ns,
                        struct xrt_space_relation *out_relation)
{
	struct gats_hmd *gats = gats_hmd(xdev);

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		GATS_ERROR(gats, "unknown input name");
		return;
	}

	*out_relation = gats->tracker_relation; // you can change this using the debug gui
}

static void
gats_hmd_get_view_poses(struct xrt_device *xdev,
                      const struct xrt_vec3 *default_eye_relation,
                      uint64_t at_timestamp_ns,
                      uint32_t view_count,
                      struct xrt_space_relation *out_head_relation,
                      struct xrt_fov *out_fovs,
                      struct xrt_pose *out_poses)
{
	struct gats_hmd *gats = gats_hmd(xdev);

	u_device_get_view_poses(xdev, default_eye_relation, at_timestamp_ns, view_count, out_head_relation, out_fovs,
	                        out_poses);
}

/*
 *
 * Create function.
 *
 */

struct xrt_device *
gats_hmd_create(const cJSON *config_json)
{
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct gats_hmd *gats = U_DEVICE_ALLOCATE(struct gats_hmd, flags, 1, 0);

	size_t idx = 0;
	gats->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_OPAQUE;
	gats->base.hmd->blend_mode_count = idx;

	gats->base.update_inputs = gats_hmd_update_inputs;
	gats->base.get_tracked_pose = gats_hmd_get_tracked_pose;
	gats->base.get_view_poses = gats_hmd_get_view_poses;
	gats->base.destroy = gats_hmd_destroy;

	gats->config_json = config_json;
	gats->tracker_relation.pose = (struct xrt_pose)XRT_POSE_IDENTITY;
	gats->tracker_relation.relation_flags = (enum xrt_space_relation_flags)(
	    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
	    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT | XRT_SPACE_RELATION_POSITION_TRACKED_BIT);
	gats->log_level = debug_get_log_option_gats_log();

	snprintf(gats->base.str, XRT_DEVICE_NAME_LEN, "Ghost and the Shell");
	snprintf(gats->base.serial, XRT_DEVICE_NAME_LEN, "Ghost and the Shell");

	gats->base.name = XRT_DEVICE_GENERIC_HMD;
	gats->base.device_type = XRT_DEVICE_TYPE_HMD;
	gats->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;
	gats->base.orientation_tracking_supported = true;
	gats->base.position_tracking_supported = false;

	gats->base.hmd->screens[0].nominal_frame_interval_ns = time_s_to_ns(1.0f / 60.0f);

	// in radians
	const double hFOV = 90 * (M_PI / 180.0);
	const double vFOV = 90 * (M_PI / 180.0);
	// center of projection
	const double hCOP = 0.5;
	const double vCOP = 0.5;
	if (
	    /* right eye */
	    !math_compute_fovs(1, hCOP, hFOV, 1, vCOP, vFOV, &gats->base.hmd->distortion.fov[1]) ||
	    /*
	     * left eye - same as right eye, except the horizontal center of projection is moved in the opposite
	     * direction now
	     */
	    !math_compute_fovs(1, 1.0 - hCOP, hFOV, 1, vCOP, vFOV, &gats->base.hmd->distortion.fov[0])) {
		// If those failed, it means our math was impossible.
		GATS_ERROR(gats, "Failed to setup basic device info");
		gats_hmd_destroy(&gats->base);
		return NULL;
	}
	const int panel_w = 1920;
	const int panel_h = 1920;

	// Single "screen" (always the case)
	gats->base.hmd->screens[0].w_pixels = panel_w * 2;
	gats->base.hmd->screens[0].h_pixels = panel_h;

	// Left, Right
	for (uint8_t eye = 0; eye < 2; ++eye) {
		gats->base.hmd->views[eye].display.w_pixels = panel_w;
		gats->base.hmd->views[eye].display.h_pixels = panel_h;
		gats->base.hmd->views[eye].viewport.x_pixels = 0;
		gats->base.hmd->views[eye].viewport.y_pixels = 0;
		gats->base.hmd->views[eye].viewport.w_pixels = panel_w;
		gats->base.hmd->views[eye].viewport.h_pixels = panel_h;
		// if rotation is not identity, the dimensions can get more complex.
		gats->base.hmd->views[eye].rot = u_device_rotation_ident;
	}
	// left eye starts at x=0, right eye starts at x=panel_width
	gats->base.hmd->views[0].viewport.x_pixels = 0;
	gats->base.hmd->views[1].viewport.x_pixels = panel_w;

	// Distortion information, fills in xdev->compute_distortion().
	u_distortion_mesh_set_none(&gats->base);

	return &gats->base;
}
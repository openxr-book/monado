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
 * Create function.
 *
 */

struct xrt_device *
gats_hmd_create(const cJSON *config_json)
{
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct gats_hmd *gats = U_DEVICE_ALLOCATE(struct gats_hmd, flags, 1, 0);

	gats->config_json = config_json;

	gats->log_level = debug_get_log_option_gats_log();
	GATS_DEBUG(gats, "Called!");

	//gats->base.compute_distortion = ns_mesh_calc;
/*
	ns->base.update_inputs = ns_hmd_update_inputs;
	ns->base.get_tracked_pose = ns_hmd_get_tracked_pose;
	ns->base.get_view_poses = ns_hmd_get_view_poses;
	ns->base.destroy = ns_hmd_destroy;
	ns->base.name = XRT_DEVICE_GENERIC_HMD;
	math_pose_identity(&ns->no_tracker_relation.pose);
	ns->no_tracker_relation.relation_flags = (enum xrt_space_relation_flags)(
	    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
	    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);
	// Appeases the inner workings of Monado for when there's no head tracker and we're giving a fake pose through
	// the debug gui
	ns->base.orientation_tracking_supported = true;
	ns->base.position_tracking_supported = true;
	ns->base.device_type = XRT_DEVICE_TYPE_HMD;


	// Print name.
	snprintf(ns->base.str, XRT_DEVICE_NAME_LEN, "North Star");
	snprintf(ns->base.serial, XRT_DEVICE_NAME_LEN, "North Star");
	// Setup input.
	ns->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;

	struct u_extents_2d exts;

	// info.w_meters = 0.0588f * 2.0f;
	// info.h_meters = 0.0655f;

	// one NS screen is 1440px wide, but there are two of them.
	exts.w_pixels = 1440 * 2;
	// Both NS screens are 1600px tall
	exts.h_pixels = 1600;

	u_extents_2d_split_side_by_side(&ns->base, &exts);

	// Setup variable tracker.
	u_var_add_root(ns, "North Star", true);
	u_var_add_pose(ns, &ns->no_tracker_relation.pose, "pose");
	ns->base.orientation_tracking_supported = true;
	ns->base.device_type = XRT_DEVICE_TYPE_HMD;

	size_t idx = 0;
	// Preferred; almost all North Stars (as of early 2021) are see-through.
	ns->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_ADDITIVE;

	// XRT_BLEND_MODE_OPAQUE is not preferred and kind of a lie, but you can totally use North Star for VR apps,
	// despite its see-through display. And there's nothing stopping you from covering up the outside of the
	// reflector, turning it into an opaque headset. As most VR apps I've encountered require BLEND_MODE_OPAQUE to
	// be an option, we need to support it.
	ns->base.hmd->blend_modes[idx++] = XRT_BLEND_MODE_OPAQUE;

	// Not supporting ALPHA_BLEND for now, because I know nothing about it, have no reason to use it, and want to
	// avoid unintended consequences. As soon as you have a specific reason to support it, go ahead and support it.
	ns->base.hmd->blend_mode_count = idx;

	uint64_t start;
	uint64_t end;

	start = os_monotonic_get_ns();
	u_distortion_mesh_fill_in_compute(&ns->base);
	end = os_monotonic_get_ns();

	float diff = (end - start);
	diff /= U_TIME_1MS_IN_NS;

	NS_DEBUG(ns, "Filling mesh took %f ms", diff);

*/
	return &gats->base;
}
// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  IPC Client HMD device.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup ipc_client
 */


#include "xrt/xrt_device.h"

#include "os/os_time.h"

#include "math/m_api.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_distortion_mesh.h"

#include "client/ipc_client.h"
#include "client/ipc_client_connection.h"
#include "ipc_client_generated.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*
 *
 * Structs and defines.
 *
 */

/*!
 * An IPC client proxy for an HMD @ref xrt_device and @ref ipc_client_xdev.
 * Using a typedef reduce impact of refactor change.
 *
 * @implements ipc_client_xdev
 * @ingroup ipc_client
 */
typedef struct ipc_client_xdev ipc_client_hmd_t;


/*
 *
 * Helpers.
 *
 */

static inline ipc_client_hmd_t *
ipc_client_hmd(struct xrt_device *xdev)
{
	return (ipc_client_hmd_t *)xdev;
}

static void
call_get_view_poses_raw(ipc_client_hmd_t *ich,
                        const struct xrt_vec3 *default_eye_relation,
                        uint64_t at_timestamp_ns,
                        uint32_t view_count,
                        struct xrt_space_relation *out_head_relation,
                        struct xrt_fov *out_fovs,
                        struct xrt_pose *out_poses)
{
	struct ipc_connection *ipc_c = ich->ipc_c;
	xrt_result_t xret;

	ipc_client_connection_lock(ipc_c);

	// Using the raw send helper is the only one that is required.
	xret = ipc_send_device_get_view_poses_locked( //
	    ipc_c,                                    //
	    ich->device_id,                           //
	    default_eye_relation,                     //
	    at_timestamp_ns,                          //
	    view_count);                              //
	IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_send_device_get_view_poses_locked", out);

	// This is the data we get back in the provided reply.
	uint32_t returned_view_count = 0;
	struct xrt_space_relation head_relation = XRT_SPACE_RELATION_ZERO;

	// Get the reply, use the raw function helper.
	xret = ipc_receive_device_get_view_poses_locked( //
	    ipc_c,                                       //
	    &head_relation,                              //
	    &returned_view_count);                       //
	IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive_device_get_view_poses_locked", out);

	if (view_count != returned_view_count) {
		IPC_ERROR(ich->ipc_c, "Wrong view counts (sent: %u != got: %u)", view_count, returned_view_count);
		assert(false);
	}

	// We can read directly to the output variables.
	xret = ipc_receive(&ipc_c->imc, out_fovs, sizeof(struct xrt_fov) * view_count);
	IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive(1)", out);

	// We can read directly to the output variables.
	xret = ipc_receive(&ipc_c->imc, out_poses, sizeof(struct xrt_pose) * view_count);
	IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive(2)", out);

	/*
	 * Finally set the head_relation that we got in the reply, mostly to
	 * demonstrate that you can use the reply struct in such a way.
	 */
	*out_head_relation = head_relation;

out:
	ipc_client_connection_unlock(ipc_c);
}


/*
 *
 * Member functions
 *
 */

static void
ipc_client_hmd_destroy(struct xrt_device *xdev)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);

	// Remove the variable tracking.
	u_var_remove_root(ich);

	// We do not own these, so don't free them.
	ich->base.inputs = NULL;
	ich->base.outputs = NULL;

	// Free this device with the helper.
	u_device_free(&ich->base);
}

static void
ipc_client_hmd_update_inputs(struct xrt_device *xdev)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);

	xrt_result_t xret = ipc_call_device_update_input(ich->ipc_c, ich->device_id);
	IPC_CHK_ONLY_PRINT(ich->ipc_c, xret, "ipc_call_device_update_input");
}

static void
ipc_client_hmd_get_tracked_pose(struct xrt_device *xdev,
                                enum xrt_input_name name,
                                uint64_t at_timestamp_ns,
                                struct xrt_space_relation *out_relation)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);
	xrt_result_t xret;

	xret = ipc_call_device_get_tracked_pose( //
	    ich->ipc_c,                          //
	    ich->device_id,                      //
	    name,                                //
	    at_timestamp_ns,                     //
	    out_relation);                       //
	IPC_CHK_ONLY_PRINT(ich->ipc_c, xret, "ipc_call_device_get_tracked_pose");
}

static void
ipc_client_hmd_get_view_poses(struct xrt_device *xdev,
                              const struct xrt_vec3 *default_eye_relation,
                              uint64_t at_timestamp_ns,
                              uint32_t view_count,
                              struct xrt_space_relation *out_head_relation,
                              struct xrt_fov *out_fovs,
                              struct xrt_pose *out_poses)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);
	xrt_result_t xret;

	struct ipc_info_get_view_poses_2 info = {0};

	if (view_count == 2) {
		// Fast path.
		xret = ipc_call_device_get_view_poses_2( //
		    ich->ipc_c,                          //
		    ich->device_id,                      //
		    default_eye_relation,                //
		    at_timestamp_ns,                     //
		    &info);                              //
		IPC_CHK_ONLY_PRINT(ich->ipc_c, xret, "ipc_call_device_get_view_poses_2");

		*out_head_relation = info.head_relation;
		for (int i = 0; i < 2; i++) {
			out_fovs[i] = info.fovs[i];
			out_poses[i] = info.poses[i];
		}

	} else if (view_count <= IPC_MAX_RAW_VIEWS) {
		// Artificial limit.

		call_get_view_poses_raw(  //
		    ich,                  //
		    default_eye_relation, //
		    at_timestamp_ns,      //
		    view_count,           //
		    out_head_relation,    //
		    out_fovs,             //
		    out_poses);           //
	} else {
		IPC_ERROR(ich->ipc_c, "Cannot handle %u view_count, %u or less supported.", view_count,
		          (uint32_t)IPC_MAX_RAW_VIEWS);
		assert(false && !"Too large view_count!");
	}
}

static bool
ipc_client_hmd_compute_distortion(
    struct xrt_device *xdev, uint32_t view, float u, float v, struct xrt_uv_triplet *out_result)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);
	xrt_result_t xret;

	bool ret;
	xret = ipc_call_device_compute_distortion( //
	    ich->ipc_c,                            //
	    ich->device_id,                        //
	    view,                                  //
	    u,                                     //
	    v,                                     //
	    &ret,                                  //
	    out_result);                           //
	IPC_CHK_WITH_RET(ich->ipc_c, xret, "ipc_call_device_compute_distortion", false);

	return ret;
}

static bool
ipc_client_hmd_is_form_factor_available(struct xrt_device *xdev, enum xrt_form_factor form_factor)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);
	xrt_result_t xret;

	bool available = false;
	xret = ipc_call_device_is_form_factor_available( //
	    ich->ipc_c,                                  //
	    ich->device_id,                              //
	    form_factor,                                 //
	    &available);                                 //
	IPC_CHK_ONLY_PRINT(ich->ipc_c, xret, "ipc_call_device_is_form_factor_available");

	return available;
}

static enum xrt_result
ipc_client_hmd_begin_plane_detection_ext(struct xrt_device *xdev,
                                         const struct xrt_plane_detector_begin_info_ext *begin_info,
                                         uint64_t plane_detection_id,
                                         uint64_t *out_plane_detection_id)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);

	ich->ipc_c->ism->plane_begin_info_ext = *begin_info;

	xrt_result_t r = ipc_call_device_begin_plane_detection_ext(ich->ipc_c, ich->device_id, plane_detection_id,
	                                                           out_plane_detection_id);
	if (r != XRT_SUCCESS) {
		IPC_ERROR(ich->ipc_c, "Error sending hmd_begin_plane_detection_ext!");
		return r;
	}

	return XRT_SUCCESS;
}

static enum xrt_result
ipc_client_hmd_destroy_plane_detection_ext(struct xrt_device *xdev, uint64_t plane_detection_id)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);

	xrt_result_t r = ipc_call_device_destroy_plane_detection_ext(ich->ipc_c, ich->device_id, plane_detection_id);
	if (r != XRT_SUCCESS) {
		IPC_ERROR(ich->ipc_c, "Error sending destroy_plane_detection_ext!");
		return r;
	}

	return XRT_SUCCESS;
}

/*!
 * Helper function for @ref xrt_device::get_plane_detection_state.
 *
 * @public @memberof xrt_device
 */
static inline enum xrt_result
ipc_client_hmd_get_plane_detection_state_ext(struct xrt_device *xdev,
                                             uint64_t plane_detection_id,
                                             enum xrt_plane_detector_state_ext *out_state)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);

	xrt_result_t r =
	    ipc_call_device_get_plane_detection_state_ext(ich->ipc_c, ich->device_id, plane_detection_id, out_state);
	if (r != XRT_SUCCESS) {
		IPC_ERROR(ich->ipc_c, "Error sending get_plane_detection_state_ext!");
		return r;
	}

	return XRT_SUCCESS;
}

/*!
 * Helper function for @ref xrt_device::get_plane_detections.
 *
 * @public @memberof xrt_device
 */
static inline enum xrt_result
ipc_client_hmd_get_plane_detections_ext(struct xrt_device *xdev,
                                        uint64_t plane_detection_id,
                                        struct xrt_plane_detections_ext *out_detections)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);
	struct ipc_connection *ipc_c = ich->ipc_c;

	ipc_client_connection_lock(ipc_c);

	xrt_result_t xret = ipc_send_device_get_plane_detections_ext_locked(ipc_c, ich->device_id, plane_detection_id);
	IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_send_device_get_plane_detections_ext_locked", out);

	// in this case, size == count
	uint32_t location_size = 0;
	uint32_t polygon_size = 0;
	uint32_t vertex_size = 0;

	xret = ipc_receive_device_get_plane_detections_ext_locked(ipc_c, &location_size, &polygon_size, &vertex_size);
	IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive_device_get_plane_detections_ext_locked", out);


	// With no locations, the service won't send anything else
	if (location_size < 1) {
		out_detections->location_count = 0;
		goto out;
	}

	// realloc arrays in out_detections if necessary, then receive contents

	out_detections->location_count = location_size;
	if (out_detections->location_size < location_size) {
		U_ARRAY_REALLOC_OR_FREE(out_detections->locations, struct xrt_plane_detector_locations_ext,
		                        location_size);
		U_ARRAY_REALLOC_OR_FREE(out_detections->polygon_info_start_index, uint32_t, location_size);
		out_detections->location_size = location_size;
	}

	if (out_detections->polygon_info_size < polygon_size) {
		U_ARRAY_REALLOC_OR_FREE(out_detections->polygon_infos, struct xrt_plane_polygon_info_ext, polygon_size);
		out_detections->polygon_info_size = polygon_size;
	}

	if (out_detections->vertex_size < vertex_size) {
		U_ARRAY_REALLOC_OR_FREE(out_detections->vertices, struct xrt_vec2, vertex_size);
		out_detections->vertex_size = vertex_size;
	}

	if ((location_size > 0 &&
	     (out_detections->locations == NULL || out_detections->polygon_info_start_index == NULL)) ||
	    (polygon_size > 0 && out_detections->polygon_infos == NULL) ||
	    (vertex_size > 0 && out_detections->vertices == NULL)) {
		IPC_ERROR(ich->ipc_c, "Error allocating memory for plane detections!");
		out_detections->location_size = 0;
		out_detections->polygon_info_size = 0;
		out_detections->vertex_size = 0;
		xret = XRT_ERROR_IPC_FAILURE;
		goto out;
	}

	if (location_size > 0) {
		// receive location_count * locations
		xret = ipc_receive(&ipc_c->imc, out_detections->locations,
		                   sizeof(struct xrt_plane_detector_locations_ext) * location_size);
		IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive(1)", out);

		// receive location_count * polygon_info_start_index
		xret = ipc_receive(&ipc_c->imc, out_detections->polygon_info_start_index,
		                   sizeof(uint32_t) * location_size);
		IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive(2)", out);
	}


	if (polygon_size > 0) {
		// receive polygon_count * polygon_infos
		xret = ipc_receive(&ipc_c->imc, out_detections->polygon_infos,
		                   sizeof(struct xrt_plane_polygon_info_ext) * polygon_size);
		IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive(3)", out);
	}

	if (vertex_size > 0) {
		// receive vertex_count * vertices
		xret = ipc_receive(&ipc_c->imc, out_detections->vertices, sizeof(struct xrt_vec2) * vertex_size);
		IPC_CHK_WITH_GOTO(ich->ipc_c, xret, "ipc_receive(4)", out);
	}

out:
	ipc_client_connection_unlock(ipc_c);
	return xret;
}

static xrt_result_t
ipc_client_hmd_get_visibility_mask(struct xrt_device *xdev,
                                   enum xrt_visibility_mask_type type,
                                   uint32_t view_index,
                                   struct xrt_visibility_mask **out_mask)
{
	ipc_client_hmd_t *ich = ipc_client_hmd(xdev);
	struct ipc_connection *ipc_c = ich->ipc_c;
	struct xrt_visibility_mask *mask = NULL;
	xrt_result_t xret;

	ipc_client_connection_lock(ipc_c);

	xret = ipc_send_device_get_visibility_mask_locked(ipc_c, ich->device_id, type, view_index);
	IPC_CHK_WITH_GOTO(ipc_c, xret, "ipc_send_device_get_visibility_mask_locked", err_mask_unlock);

	uint32_t mask_size;
	xret = ipc_receive_device_get_visibility_mask_locked(ipc_c, &mask_size);
	IPC_CHK_WITH_GOTO(ipc_c, xret, "ipc_receive_device_get_visibility_mask_locked", err_mask_unlock);

	mask = U_CALLOC_WITH_CAST(struct xrt_visibility_mask, mask_size);
	if (mask == NULL) {
		IPC_ERROR(ich->ipc_c, "failed to allocate xrt_visibility_mask");
		goto err_mask_unlock;
	}

	xret = ipc_receive(&ipc_c->imc, mask, mask_size);
	IPC_CHK_WITH_GOTO(ipc_c, xret, "ipc_receive", err_mask_free);

	*out_mask = mask;
	ipc_client_connection_unlock(ipc_c);

	return XRT_SUCCESS;

err_mask_free:
	free(mask);
err_mask_unlock:
	ipc_client_connection_unlock(ipc_c);
	return XRT_ERROR_IPC_FAILURE;
}

/*!
 * @public @memberof ipc_client_hmd
 */
struct xrt_device *
ipc_client_hmd_create(struct ipc_connection *ipc_c, struct xrt_tracking_origin *xtrack, uint32_t device_id)
{
	struct ipc_shared_memory *ism = ipc_c->ism;
	struct ipc_shared_device *isdev = &ism->isdevs[device_id];



	enum u_device_alloc_flags flags = (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD);
	ipc_client_hmd_t *ich = U_DEVICE_ALLOCATE(ipc_client_hmd_t, flags, 0, 0);
	ich->ipc_c = ipc_c;
	ich->device_id = device_id;
	ich->base.update_inputs = ipc_client_hmd_update_inputs;
	ich->base.get_tracked_pose = ipc_client_hmd_get_tracked_pose;
	ich->base.get_view_poses = ipc_client_hmd_get_view_poses;
	ich->base.compute_distortion = ipc_client_hmd_compute_distortion;
	ich->base.begin_plane_detection_ext = ipc_client_hmd_begin_plane_detection_ext;
	ich->base.destroy_plane_detection_ext = ipc_client_hmd_destroy_plane_detection_ext;
	ich->base.get_plane_detection_state_ext = ipc_client_hmd_get_plane_detection_state_ext;
	ich->base.get_plane_detections_ext = ipc_client_hmd_get_plane_detections_ext;
	ich->base.destroy = ipc_client_hmd_destroy;
	ich->base.is_form_factor_available = ipc_client_hmd_is_form_factor_available;
	ich->base.get_visibility_mask = ipc_client_hmd_get_visibility_mask;

	// Start copying the information from the isdev.
	ich->base.tracking_origin = xtrack;
	ich->base.name = isdev->name;
	ich->device_id = device_id;

	// Print name.
	snprintf(ich->base.str, XRT_DEVICE_NAME_LEN, "%s", isdev->str);
	snprintf(ich->base.serial, XRT_DEVICE_NAME_LEN, "%s", isdev->serial);

	// Setup inputs, by pointing directly to the shared memory.
	assert(isdev->input_count > 0);
	ich->base.inputs = &ism->inputs[isdev->first_input_index];
	ich->base.input_count = isdev->input_count;

#if 0
	// Setup info.
	struct u_device_simple_info info;
	info.display.w_pixels = 1920;
	info.display.h_pixels = 1080;
	info.display.w_meters = 0.13f;
	info.display.h_meters = 0.07f;
	info.lens_horizontal_separation_meters = 0.13f / 2.0f;
	info.lens_vertical_position_meters = 0.07f / 2.0f;
	info.views[0].fov = 85.0f * (M_PI / 180.0f);
	info.views[1].fov = 85.0f * (M_PI / 180.0f);

	if (!u_device_setup_split_side_by_side(&ich->base, &info)) {
		IPC_ERROR(ich->ipc_c, "Failed to setup basic device info");
		ipc_client_hmd_destroy(&ich->base);
		return NULL;
	}
#endif
	for (int i = 0; i < XRT_MAX_DEVICE_BLEND_MODES; i++) {
		ich->base.hmd->blend_modes[i] = ipc_c->ism->hmd.blend_modes[i];
	}
	ich->base.hmd->blend_mode_count = ipc_c->ism->hmd.blend_mode_count;

	ich->base.hmd->views[0].display.w_pixels = ipc_c->ism->hmd.views[0].display.w_pixels;
	ich->base.hmd->views[0].display.h_pixels = ipc_c->ism->hmd.views[0].display.h_pixels;
	ich->base.hmd->views[1].display.w_pixels = ipc_c->ism->hmd.views[1].display.w_pixels;
	ich->base.hmd->views[1].display.h_pixels = ipc_c->ism->hmd.views[1].display.h_pixels;

	// Distortion information, fills in xdev->compute_distortion().
	u_distortion_mesh_set_none(&ich->base);

	// Setup variable tracker.
	u_var_add_root(ich, ich->base.str, true);
	u_var_add_ro_u32(ich, &ich->device_id, "device_id");

	ich->base.orientation_tracking_supported = isdev->orientation_tracking_supported;
	ich->base.position_tracking_supported = isdev->position_tracking_supported;
	ich->base.device_type = isdev->device_type;
	ich->base.hand_tracking_supported = isdev->hand_tracking_supported;
	ich->base.eye_gaze_supported = isdev->eye_gaze_supported;
	ich->base.force_feedback_supported = isdev->force_feedback_supported;
	ich->base.form_factor_check_supported = isdev->form_factor_check_supported;
	ich->base.planes_supported = isdev->planes_supported;
	ich->base.plane_capability_flags = isdev->plane_capability_flags;

	return &ich->base;
}

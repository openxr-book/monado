// Copyright 2020-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  IPC Client device.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup ipc_client
 */

#include "xrt/xrt_device.h"

#include "os/os_time.h"

#include "math/m_api.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_device.h"

#include "client/ipc_client.h"
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
 * An IPC client proxy for an controller or other non-MHD @ref xrt_device and
 * @ref ipc_client_xdev. Using a typedef reduce impact of refactor change.
 *
 * @implements ipc_client_xdev
 * @ingroup ipc_client
 */
typedef struct ipc_client_xdev ipc_client_device_t;


/*
 *
 * Functions
 *
 */

static inline ipc_client_device_t *
ipc_client_device(struct xrt_device *xdev)
{
	return (ipc_client_device_t *)xdev;
}

static void
ipc_client_device_destroy(struct xrt_device *xdev)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	// Remove the variable tracking.
	u_var_remove_root(icd);

	// We do not own these, so don't free them.
	icd->base.inputs = NULL;
	icd->base.outputs = NULL;

	// Free this device with the helper.
	u_device_free(&icd->base);
}

static void
ipc_client_device_update_inputs(struct xrt_device *xdev)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_update_input(icd->ipc_c, icd->device_id);
	IPC_CHK_ONLY_PRINT(icd->ipc_c, xret, "ipc_call_device_update_input");
}

static void
ipc_client_device_get_tracked_pose(struct xrt_device *xdev,
                                   enum xrt_input_name name,
                                   uint64_t at_timestamp_ns,
                                   struct xrt_space_relation *out_relation)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_get_tracked_pose( //
	    icd->ipc_c,                                       //
	    icd->device_id,                                   //
	    name,                                             //
	    at_timestamp_ns,                                  //
	    out_relation);                                    //
	IPC_CHK_ONLY_PRINT(icd->ipc_c, xret, "ipc_call_device_get_tracked_pose");
}

static void
ipc_client_device_get_hand_tracking(struct xrt_device *xdev,
                                    enum xrt_input_name name,
                                    uint64_t at_timestamp_ns,
                                    struct xrt_hand_joint_set *out_value,
                                    uint64_t *out_timestamp_ns)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_get_hand_tracking( //
	    icd->ipc_c,                                        //
	    icd->device_id,                                    //
	    name,                                              //
	    at_timestamp_ns,                                   //
	    out_value,                                         //
	    out_timestamp_ns);                                 //
	IPC_CHK_ONLY_PRINT(icd->ipc_c, xret, "ipc_call_device_get_hand_tracking");
}

static xrt_result_t
ipc_client_device_get_face_tracking(struct xrt_device *xdev,
                                    enum xrt_input_name facial_expression_type,
                                    struct xrt_facial_expression_set *out_value)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_get_face_tracking( //
	    icd->ipc_c,                                        //
	    icd->device_id,                                    //
	    facial_expression_type,                            //
	    out_value);                                        //
	IPC_CHK_ALWAYS_RET(icd->ipc_c, xret, "ipc_call_device_get_face_tracking");
}

static xrt_result_t
ipc_client_device_get_body_skeleton(struct xrt_device *xdev,
                                    enum xrt_input_name body_tracking_type,
                                    struct xrt_body_skeleton *out_value)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_get_body_skeleton( //
	    icd->ipc_c,                                        //
	    icd->device_id,                                    //
	    body_tracking_type,                                //
	    out_value);                                        //
	IPC_CHK_ALWAYS_RET(icd->ipc_c, xret, "ipc_call_device_get_body_skeleton");
}

static xrt_result_t
ipc_client_device_get_body_joints(struct xrt_device *xdev,
                                  enum xrt_input_name body_tracking_type,
                                  uint64_t desired_timestamp_ns,
                                  struct xrt_body_joint_set *out_value)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_get_body_joints( //
	    icd->ipc_c,                                      //
	    icd->device_id,                                  //
	    body_tracking_type,                              //
	    desired_timestamp_ns,                            //
	    out_value);                                      //
	IPC_CHK_ALWAYS_RET(icd->ipc_c, xret, "ipc_call_device_get_body_joints");
}

static xrt_result_t
ipc_client_device_set_body_tracking_fidelity_meta(struct xrt_device *xdev,
                                                  enum xrt_body_tracking_fidelity_meta new_fidelity)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_set_body_tracking_fidelity_meta( //
	    icd->ipc_c,                                                      //
	    icd->device_id,                                                  //
	    new_fidelity);                                                   //
	IPC_CHK_ALWAYS_RET(icd->ipc_c, xret, "ipc_call_device_set_body_tracking_fidelity_meta");
}

static void
ipc_client_device_get_view_poses(struct xrt_device *xdev,
                                 const struct xrt_vec3 *default_eye_relation,
                                 uint64_t at_timestamp_ns,
                                 uint32_t view_count,
                                 struct xrt_space_relation *out_head_relation,
                                 struct xrt_fov *out_fovs,
                                 struct xrt_pose *out_poses)
{
	// Empty
	assert(false);
}

static void
ipc_client_device_set_output(struct xrt_device *xdev, enum xrt_output_name name, const union xrt_output_value *value)
{
	ipc_client_device_t *icd = ipc_client_device(xdev);

	xrt_result_t xret = ipc_call_device_set_output(icd->ipc_c, icd->device_id, name, value);
	IPC_CHK_ONLY_PRINT(icd->ipc_c, xret, "ipc_call_device_set_output");
}

static xrt_result_t
ipc_client_device_get_visibility_mask(struct xrt_device *xdev,
                                      enum xrt_visibility_mask_type type,
                                      uint32_t view_index,
                                      struct xrt_visibility_mask **out_mask)
{
	assert(false);
	return XRT_ERROR_IPC_FAILURE;
}

/*!
 * @public @memberof ipc_client_device
 */
struct xrt_device *
ipc_client_device_create(struct ipc_connection *ipc_c, struct xrt_tracking_origin *xtrack, uint32_t device_id)
{
	// Helpers.
	struct ipc_shared_memory *ism = ipc_c->ism;
	struct ipc_shared_device *isdev = &ism->isdevs[device_id];

	// Allocate and setup the basics.
	enum u_device_alloc_flags flags = (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD);
	ipc_client_device_t *icd = U_DEVICE_ALLOCATE(ipc_client_device_t, flags, 0, 0);
	icd->ipc_c = ipc_c;
	icd->base.update_inputs = ipc_client_device_update_inputs;
	icd->base.get_tracked_pose = ipc_client_device_get_tracked_pose;
	icd->base.get_hand_tracking = ipc_client_device_get_hand_tracking;
	icd->base.get_face_tracking = ipc_client_device_get_face_tracking;
	icd->base.get_body_skeleton = ipc_client_device_get_body_skeleton;
	icd->base.get_body_joints = ipc_client_device_get_body_joints;
	icd->base.set_body_tracking_fidelity_meta = ipc_client_device_set_body_tracking_fidelity_meta;
	icd->base.get_view_poses = ipc_client_device_get_view_poses;
	icd->base.set_output = ipc_client_device_set_output;
	icd->base.get_visibility_mask = ipc_client_device_get_visibility_mask;
	icd->base.destroy = ipc_client_device_destroy;

	// Start copying the information from the isdev.
	icd->base.tracking_origin = xtrack;
	icd->base.name = isdev->name;
	icd->device_id = device_id;

	// Print name.
	snprintf(icd->base.str, XRT_DEVICE_NAME_LEN, "%s", isdev->str);
	snprintf(icd->base.serial, XRT_DEVICE_NAME_LEN, "%s", isdev->serial);

	// Setup inputs, by pointing directly to the shared memory.
	assert(isdev->input_count > 0);
	icd->base.inputs = &ism->inputs[isdev->first_input_index];
	icd->base.input_count = isdev->input_count;

	// Setup outputs, if any point directly into the shared memory.
	icd->base.output_count = isdev->output_count;
	if (isdev->output_count > 0) {
		icd->base.outputs = &ism->outputs[isdev->first_output_index];
	} else {
		icd->base.outputs = NULL;
	}

	if (isdev->binding_profile_count > 0) {
		icd->base.binding_profiles =
		    U_TYPED_ARRAY_CALLOC(struct xrt_binding_profile, isdev->binding_profile_count);
		icd->base.binding_profile_count = isdev->binding_profile_count;
	}

	for (size_t i = 0; i < isdev->binding_profile_count; i++) {
		struct xrt_binding_profile *xbp = &icd->base.binding_profiles[i];
		struct ipc_shared_binding_profile *isbp =
		    &ism->binding_profiles[isdev->first_binding_profile_index + i];

		xbp->name = isbp->name;
		if (isbp->input_count > 0) {
			xbp->inputs = &ism->input_pairs[isbp->first_input_index];
			xbp->input_count = isbp->input_count;
		}
		if (isbp->output_count > 0) {
			xbp->outputs = &ism->output_pairs[isbp->first_output_index];
			xbp->output_count = isbp->output_count;
		}
	}

	// Setup variable tracker.
	u_var_add_root(icd, icd->base.str, true);
	u_var_add_ro_u32(icd, &icd->device_id, "device_id");

	icd->base.orientation_tracking_supported = isdev->orientation_tracking_supported;
	icd->base.position_tracking_supported = isdev->position_tracking_supported;
	icd->base.hand_tracking_supported = isdev->hand_tracking_supported;
	icd->base.eye_gaze_supported = isdev->eye_gaze_supported;
	icd->base.face_tracking_supported = isdev->face_tracking_supported;
	icd->base.body_tracking_supported = isdev->body_tracking_supported;
	icd->base.force_feedback_supported = isdev->force_feedback_supported;
	icd->base.stage_supported = isdev->stage_supported;

	icd->base.device_type = isdev->device_type;
	return &icd->base;
}

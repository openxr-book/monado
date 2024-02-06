// Copyright 2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Holds automated device related functions
 * @author Simon Zeni <simon.zeni@collabora.com>
 * @ingroup oxr_main
 */

#include "util/u_var.h"
#include "xrt/xrt_session.h"

#include "oxr_objects.h"
#include "oxr_logger.h"

#include <assert.h>
#include <stdio.h>

struct automated_device
{
	struct xrt_device base;

	bool active;
	struct xrt_pose pose;
	struct oxr_space *space;
};

static struct automated_device *
get_automated_device(struct xrt_device *xdev)
{
	assert(oxr_automation_device_is_automated(xdev));
	return (struct automated_device *)xdev;
}

static void
automated_device_update_inputs(struct xrt_device *xdev)
{}

static void
automated_device_destroy(struct xrt_device *xdev)
{
	struct automated_device *dev = get_automated_device(xdev);

	// Remove the variable tracking.
	u_var_remove_root(dev);

	free(dev);
}

static struct xrt_device *
automated_device_create(struct oxr_interaction_profile *profile, enum xrt_device_type type)
{
	struct automated_device *dev = U_DEVICE_ALLOCATE(struct automated_device, 0, profile->binding_count, 0);
	if (dev == NULL) {
		return NULL;
	}

	dev->base.name = profile->xname;
	dev->base.device_type = type;

	snprintf(dev->base.str, sizeof(dev->base.str), "%s (automated)", profile->localized_name);
	snprintf(dev->base.serial, sizeof(dev->base.serial), "%s (automated)", profile->localized_name);

	for (size_t i = 0; i < profile->binding_count; ++i) {
		struct oxr_binding *b = &profile->bindings[i];
		dev->base.inputs[i].name = b->input;
	}

	static const struct xrt_tracking_origin origin = {
	    .name = "Automated tracking origin",
	    .type = XRT_TRACKING_TYPE_OTHER,
	    .offset = XRT_POSE_IDENTITY,
	};

	dev->base.tracking_origin = (struct xrt_tracking_origin *)&origin;

	dev->base.hand_tracking_supported = false;
	dev->base.orientation_tracking_supported = true;
	dev->base.position_tracking_supported = true;

	dev->base.update_inputs = automated_device_update_inputs;
	dev->base.destroy = automated_device_destroy;

	u_var_add_root(dev, dev->base.str, true);

	return &dev->base;
}

bool
oxr_automation_device_is_automated(struct xrt_device *xdev)
{
	if (xdev == NULL) {
		return false;
	}

	// TODO xrt_device_interface
	return xdev->update_inputs == automated_device_update_inputs && xdev->destroy == automated_device_destroy;
}

static enum xrt_device_type
device_type_from_subaction_path(enum oxr_subaction_path sp)
{
	switch (sp) {

	case OXR_SUB_ACTION_PATH_HEAD: return XRT_DEVICE_TYPE_HMD;
	case OXR_SUB_ACTION_PATH_LEFT: return XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER;
	case OXR_SUB_ACTION_PATH_RIGHT: return XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER;
	// TODO: are gamepad and eyes in the scope of XR_EXT_conformance_automation ?
	case OXR_SUB_ACTION_PATH_GAMEPAD:
	case OXR_SUB_ACTION_PATH_EYES:
	case OXR_SUB_ACTION_PATH_USER:
	default: return XRT_DEVICE_TYPE_UNKNOWN;
	}
}

XrResult
oxr_automation_set_input_device_active(
    struct oxr_logger *log, struct oxr_session *sess, XrPath interactionProfile, XrPath topLevelPath, XrBool32 isActive)
{
	struct oxr_instance *inst = sess->sys->inst;

	struct oxr_interaction_profile *p = oxr_profile_get_or_create(log, inst, interactionProfile);
	if (p == NULL) {
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "failed to get interaction profile");
	}

	enum oxr_subaction_path sp = 0;
	if (!oxr_get_subaction_path_from_path(log, inst, topLevelPath, &sp)) {
		return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "topLevelPath is not a valid subaction_path");
	}

	// Find a matching automated device if it exists
	enum xrt_device_type type = device_type_from_subaction_path(sp);
	if (type == XRT_DEVICE_TYPE_UNKNOWN) {
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "no device type is matching the subaction path");
	}

	struct xrt_device *xdev = NULL;

	size_t idx;
	for (idx = 0; idx < sess->sys->xsysd->xdev_count; idx++) {
		struct xrt_device *tmp = sess->sys->xsysd->xdevs[idx];
		if (tmp->name == p->xname && tmp->device_type == type && oxr_automation_device_is_automated(tmp)) {
			xdev = tmp;
			break;
		}
	}

	if (xdev == NULL) {
		xdev = automated_device_create(p, type);
		if (xdev == NULL) {
			return oxr_error(log, XR_ERROR_OUT_OF_MEMORY, "failed to allocate automated device");
		}

		// TODO: Find a better way to introduce a new device. When running in service mode, the server
		// Doesn't know about this device and that could lead to errors when the server introduces a new device
		// that overlaps with this one.
		// But XR_EXT_conformance_automation is not meant to be used in prod, so Here be dragons!
		idx = sess->sys->xsysd->xdev_count;
		sess->sys->xsysd->xdevs[sess->sys->xsysd->xdev_count++] = xdev;
	}

	struct automated_device *dev = get_automated_device(xdev);
	dev->active = isActive;

	oxr_log(log, "automated device '%s' (%s) is %s", xdev->str, xdev->serial, isActive ? "active" : "inactive");

	for (size_t i = 0; i < p->binding_count; ++i) {
		struct oxr_binding *b = &p->bindings[i];

		if (b->subaction_path != sp) {
			continue;
		}

		xdev->inputs[i].active = isActive;
		xdev->inputs[i].timestamp = os_monotonic_get_ns();

		for (size_t j = 0; j < b->key_count; ++j) {
			struct oxr_action_attachment *a = NULL;
			oxr_session_get_action_attachment(sess, b->keys[j], &a);

			switch (sp) {
			case OXR_SUB_ACTION_PATH_HEAD: a->head.current.active = isActive; break;
			case OXR_SUB_ACTION_PATH_LEFT: a->head.current.active = isActive; break;
			case OXR_SUB_ACTION_PATH_RIGHT: a->head.current.active = isActive; break;
			default: return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "subaction path not supported");
			}
		}
	}

	if (isActive) {
		// If the automated device is active, we should use it
		switch (type) {
		case XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER: sess->sys->dynamic_roles_cache.left = idx; break;
		case XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER: sess->sys->dynamic_roles_cache.right = idx; break;
		default: break;
		}

		// TODO: This messes up the synchronization between the client and the server when running in service
		// mode (see the comment near `automated_device_create`.
		sess->sys->dynamic_roles_cache.generation_id++;
	} else {
		// TODO: should we put back any existing device in place?
		switch (type) {
		case XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER: sess->sys->dynamic_roles_cache.left = -1; break;
		case XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER: sess->sys->dynamic_roles_cache.right = -1; break;
		default: break;
		}
	}

	return oxr_session_update_action_bindings(log, sess);
}

static XrResult
find_device(struct oxr_logger *log, struct oxr_session *sess, XrPath topLevelPath, struct xrt_device **out_xdev)
{
	struct oxr_instance *inst = sess->sys->inst;

	enum oxr_subaction_path sp = 0;
	if (!oxr_get_subaction_path_from_path(log, inst, topLevelPath, &sp)) {
		return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "topLevelPath is not a valid subaction_path");
	}

	enum xrt_device_type type = device_type_from_subaction_path(sp);

	struct xrt_device *xdev = NULL;
	switch (type) {
	case XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER: xdev = GET_XDEV_BY_ROLE(sess->sys, left); break;
	case XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER: xdev = GET_XDEV_BY_ROLE(sess->sys, right); break;
	default: return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "topLevelPath not supported");
	}

	if (!oxr_automation_device_is_automated(xdev)) {
		// TODO could we automate regular devices?
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "device '%s' is not automated", xdev->str);
	}

	*out_xdev = xdev;
	return XR_SUCCESS;
}

static XrResult
find_input(struct oxr_logger *log,
           struct oxr_session *sess,
           XrPath topLevelPath,
           XrPath inputSourcePath,
           struct xrt_input **out_input)
{
	struct xrt_device *xdev;
	XrResult res = find_device(log, sess, topLevelPath, &xdev);
	if (res != XR_SUCCESS) {
		return res;
	}

	struct oxr_interaction_profile *p;
	oxr_find_profile_for_device(log, sess, xdev, &p);
	if (p == NULL) {
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "could not find interaction profile for device '%s'",
		                 xdev->str);
	}

	struct oxr_binding *binding = NULL;
	for (size_t i = 0; i < p->binding_count; ++i) {
		struct oxr_binding *b = &p->bindings[i];
		for (uint32_t j = 0; j < b->path_count; ++j) {
			if (b->paths[j] == inputSourcePath) {
				binding = b;
			}
		}
	}

	if (binding == NULL) {
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "no binding found for inputSourcePath");
	}

	struct xrt_input *input = NULL;
	if (!oxr_xdev_find_input(xdev, binding->input, &input)) {
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "could not find binding input for device '%s'",
		                 xdev->str);
	}

	*out_input = input;
	return XR_SUCCESS;
}

XrResult
oxr_automation_set_input_device_state_boolean(
    struct oxr_logger *log, struct oxr_session *sess, XrPath topLevelPath, XrPath inputSourcePath, XrBool32 state)
{
	struct xrt_input *input;
	XrResult res = find_input(log, sess, topLevelPath, inputSourcePath, &input);

	if (res != XR_SUCCESS) {
		return res;
	}

	input->timestamp = os_monotonic_get_ns();
	input->value.boolean = state;

	return XR_SUCCESS;
}

XrResult
oxr_automation_set_input_device_state_float(
    struct oxr_logger *log, struct oxr_session *sess, XrPath topLevelPath, XrPath inputSourcePath, float state)
{
	struct xrt_input *input;
	XrResult res = find_input(log, sess, topLevelPath, inputSourcePath, &input);

	if (res != XR_SUCCESS) {
		return res;
	}

	input->timestamp = os_monotonic_get_ns();
	input->value.vec1.x = state;

	return XR_SUCCESS;
}

XrResult
oxr_automation_set_input_device_state_vec2(
    struct oxr_logger *log, struct oxr_session *sess, XrPath topLevelPath, XrPath inputSourcePath, XrVector2f state)
{
	struct xrt_input *input;
	XrResult res = find_input(log, sess, topLevelPath, inputSourcePath, &input);

	if (res != XR_SUCCESS) {
		return res;
	}

	input->timestamp = os_monotonic_get_ns();
	input->value.vec2.x = state.x;
	input->value.vec2.y = state.y;

	return XR_SUCCESS;
}

XrResult
oxr_automation_set_input_device_location(struct oxr_logger *log,
                                         struct oxr_session *sess,
                                         XrPath topLevelPath,
                                         XrPath inputSourcePath,
                                         struct oxr_space *space,
                                         struct xrt_pose pose)
{
	// TODO: this assumes a single space/pose for any inputSourcePath. This is wrong but the OpenXR CTS only
	// tests for a single inputSourcePath (grip).
	// TODO: store multiple poses for different spaces

	struct xrt_device *xdev;
	XrResult res = find_device(log, sess, topLevelPath, &xdev);
	if (res != XR_SUCCESS) {
		return res;
	}

	struct automated_device *dev = get_automated_device(xdev);
	dev->pose = pose;
	dev->space = space;

	return XR_SUCCESS;
}

XrResult
oxr_automation_locate_space(struct oxr_logger *log,
                            struct oxr_space *space,
                            struct oxr_space *base_space,
                            uint64_t ts_ns,
                            struct xrt_space_relation *out_relation)
{
	// TODO: are those validated elsewhere?
	assert(space->space_type == OXR_SPACE_TYPE_ACTION);
	assert(base_space->space_type != OXR_SPACE_TYPE_ACTION);

	struct automated_device *dev = get_automated_device(space->action.xdev);

	// TODO: we should be able to store multiple spaces in `xrSetInputDeviceLocationEXT`
	// See oxr_automation_set_input_device_location
	if (base_space != dev->space) {
		return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "automated device '%s' space mismatch",
		                 dev->base.str);
	}

	out_relation->pose = dev->pose;

	// TODO: should we compute those?
	struct xrt_vec3 zero = XRT_VEC3_ZERO;
	out_relation->linear_velocity = zero;
	out_relation->angular_velocity = zero;

	if (dev->active) {
		out_relation->relation_flags = (enum xrt_space_relation_flags)(
		    XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_POSITION_VALID_BIT |
		    XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT | XRT_SPACE_RELATION_POSITION_TRACKED_BIT);
	} else {
		out_relation->relation_flags = 0;
	}

	return XR_SUCCESS;
}

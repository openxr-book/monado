// Copyright 2018-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  File for verifying app input into api functions.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup oxr_api
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_thing, THING, name, lookup)                             \
	do {                                                                                                           \
		oxr_log_init(log, name);                                                                               \
		if (thing == XR_NULL_HANDLE) {                                                                         \
			return oxr_error(log, XR_ERROR_HANDLE_INVALID, "(" #thing " == NULL)");                        \
		}                                                                                                      \
		new_thing = (struct oxr_thing *)((uintptr_t)thing);                                                    \
		if (new_thing->handle.debug != OXR_XR_DEBUG_##THING) {                                                 \
			return oxr_error(log, XR_ERROR_HANDLE_INVALID, "(" #thing " == %p)", (void *)new_thing);       \
		}                                                                                                      \
		if (new_thing->handle.state != OXR_HANDLE_STATE_LIVE) {                                                \
			return oxr_error(log, XR_ERROR_HANDLE_INVALID, "(" #thing " == %p) state == %s",               \
			                 (void *)new_thing, oxr_handle_state_to_string(new_thing->handle.state));      \
		}                                                                                                      \
		oxr_log_set_instance(log, lookup);                                                                     \
	} while (0)

#define OXR_VERIFY_SET(log, arg, new_arg, oxr_thing, THING)                                                            \
	do {                                                                                                           \
		if (arg == XR_NULL_HANDLE) {                                                                           \
			return oxr_error(log, XR_ERROR_HANDLE_INVALID, "(" #arg " == NULL)");                          \
		}                                                                                                      \
		new_arg = (struct oxr_thing *)((uintptr_t)arg);                                                        \
		if (new_arg->handle.debug != OXR_XR_DEBUG_##THING) {                                                   \
			return oxr_error(log, XR_ERROR_HANDLE_INVALID, "(" #arg " == %p)", (void *)new_arg);           \
		}                                                                                                      \
	} while (0)


/*!
 * @addtogroup oxr_api
 * @{
 */

// clang-format off
#define OXR_VERIFY_INSTANCE_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_instance, INSTANCE, name, new_thing)
#define OXR_VERIFY_MESSENGER_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_messenger, MESSENGER, name, new_thing->inst)
#define OXR_VERIFY_SESSION_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_session, SESSION, name, new_thing->sys->inst)
#define OXR_VERIFY_SPACE_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_space, SPACE, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_ACTION_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_action, ACTION, name, new_thing->act_set->inst)
#define OXR_VERIFY_SWAPCHAIN_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_swapchain, SWAPCHAIN, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_ACTIONSET_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_action_set, ACTIONSET, name, new_thing->inst)
#define OXR_VERIFY_HAND_TRACKER_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_hand_tracker, HTRACKER, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_FORCE_FEEDBACK_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_force_feedback, FFB, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_PASSTHROUGH_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_passthrough, PASSTHROUGH, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_PASSTHROUGH_LAYER_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_passthrough_layer, PASSTHROUGH_LAYER, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_FACE_TRACKER_HTC_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_facial_tracker_htc, FTRACKER, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_BODY_TRACKER_FB_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_body_tracker_fb, BTRACKER, name, new_thing->sess->sys->inst)
#define OXR_VERIFY_XDEVLIST_AND_INIT_LOG(log, thing, new_thing, name) \
	OXR_VERIFY_AND_SET_AND_INIT(log, thing, new_thing, oxr_xdev_list, XDEVLIST, name, new_thing->sess->sys->inst)
// clang-format on

#define OXR_VERIFY_INSTANCE_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_instance, INSTANCE);
#define OXR_VERIFY_MESSENGER_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_messenger, MESSENGER);
#define OXR_VERIFY_SESSION_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_session, SESSION);
#define OXR_VERIFY_SPACE_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_space, SPACE);
#define OXR_VERIFY_ACTION_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_action, ACTION);
#define OXR_VERIFY_SWAPCHAIN_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_swapchain, SWAPCHAIN);
#define OXR_VERIFY_ACTIONSET_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_action_set, ACTIONSET);
#define OXR_VERIFY_XDEVLIST_NOT_NULL(log, arg, new_arg) OXR_VERIFY_SET(log, arg, new_arg, oxr_xdev_list, XDEVLIST);

/*!
 * Checks if a required extension is enabled.
 *
 * mixed_case_name should be the extension name without the XR_ prefix.
 */
#define OXR_VERIFY_EXTENSION(log, inst, mixed_case_name)                                                               \
	do {                                                                                                           \
		if (!(inst)->extensions.mixed_case_name) {                                                             \
			return oxr_error((log), XR_ERROR_FUNCTION_UNSUPPORTED,                                         \
			                 "Requires XR_" #mixed_case_name " extension enabled");                        \
		}                                                                                                      \
	} while (false)

/*!
 * Checks if either one of two required extensions is enabled.
 *
 * mixed_case_name should be the extension name without the XR_ prefix.
 */
#define OXR_VERIFY_EXTENSIONS_OR(log, inst, mixed_case_name1, mixed_case_name2)                                        \
	do {                                                                                                           \
		if (!(inst)->extensions.mixed_case_name1 && !(inst)->extensions.mixed_case_name2) {                    \
			return oxr_error((log), XR_ERROR_FUNCTION_UNSUPPORTED,                                         \
			                 "Requires XR_" #mixed_case_name1 "or XR_" #mixed_case_name2                   \
			                 " extension enabled");                                                        \
		}                                                                                                      \
	} while (false)

/*!
 * A condition if the instance is created with at least a given OpenXR version
 */
#define OXR_API_VERSION_AT_LEAST(inst, major, minor)                                                                   \
	((inst)->openxr_version.major_minor >= XR_MAKE_VERSION(major, minor, 0))

/*!
 * Checks if the instance is created with at least a given OpenXR version
 */
#define OXR_VERIFY_API_VERSION_AT_LEAST(log, inst, major, minor)                                                       \
	do {                                                                                                           \
		if (!OXR_VERSION_AT_LEAST(inst, major, minor)) {                                                       \
			return oxr_error((log), XR_ERROR_FUNCTION_UNSUPPORTED, "Requires OpenXR version %d.%d.x",      \
			                 major, minor);                                                                \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_NOT_NULL(log, arg)                                                                              \
	do {                                                                                                           \
		if (arg == NULL) {                                                                                     \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "(" #arg " == NULL)");                      \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_NOT_ZERO(log, arg)                                                                              \
	do {                                                                                                           \
		if (arg == 0) {                                                                                        \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "(" #arg " == 0) must be non-zero");        \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_ZERO(log, arg)                                                                                  \
	do {                                                                                                           \
		if (arg != 0) {                                                                                        \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "(" #arg " == 0) must be zero");            \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_TYPE_CAN_BE_NULL(log, arg, type_enum)                                                           \
	do {                                                                                                           \
		if (arg != NULL && arg->type != type_enum) {                                                           \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "(" #arg "->type == %u)", arg->type);       \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_TYPE_AND_NOT_NULL(log, arg, type_enum)                                                          \
	do {                                                                                                           \
		if (arg == NULL) {                                                                                     \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE, "(" #arg " == NULL)");                      \
		}                                                                                                      \
		OXR_VERIFY_ARG_TYPE_CAN_BE_NULL(log, arg, type_enum);                                                  \
	} while (false)

/*!
 * Must only be used with full typed arrays, aka non-basetyped arrays like that
 * passed into xrEnumerateSwapchainImages.
 */
#define OXR_VERIFY_ARG_ARRAY_ELEMENT_TYPE(log, array, index, type_enum)                                                \
	do {                                                                                                           \
		if ((array)[index].type != type_enum) {                                                                \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                             \
			                 "(" #array "[%u]->type == 0x%08x) expected 0x%08x", index,                    \
			                 (array)[index].type, type_enum);                                              \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_SUBACTION_PATHS(log, count, paths)                                                                  \
	do {                                                                                                           \
		if (count > 0 && paths == NULL) {                                                                      \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                             \
			                 "(" #count ") is not zero but " #paths " is NULL");                           \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_SINGLE_LEVEL_FIXED_LENGTH_PATH(log, path)                                                       \
	do {                                                                                                           \
		XrResult verify_ret = oxr_verify_fixed_size_single_level_path(log, path, ARRAY_SIZE(path), #path);     \
		if (verify_ret != XR_SUCCESS) {                                                                        \
			return verify_ret;                                                                             \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_ARG_LOCALIZED_NAME(log, string)                                                                     \
	do {                                                                                                           \
		XrResult verify_ret = oxr_verify_localized_name(log, string, ARRAY_SIZE(string), #string);             \
		if (verify_ret != XR_SUCCESS) {                                                                        \
			return verify_ret;                                                                             \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_POSE(log, p)                                                                                        \
	do {                                                                                                           \
		if (!math_quat_validate((struct xrt_quat *)&p.orientation)) {                                          \
			return oxr_error(log, XR_ERROR_POSE_INVALID, "(" #p ".orientation) is not a valid quat");      \
		}                                                                                                      \
                                                                                                                       \
		if (!math_vec3_validate((struct xrt_vec3 *)&p.position)) {                                             \
			return oxr_error(log, XR_ERROR_POSE_INVALID, "(" #p ".position) is not valid");                \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_VIEW_CONFIG_TYPE(log, inst, view_conf)                                                              \
	do {                                                                                                           \
		XrResult verify_ret = oxr_verify_view_config_type(log, inst, view_conf, #view_conf);                   \
		if (verify_ret != XR_SUCCESS) {                                                                        \
			return verify_ret;                                                                             \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_VIEW_INDEX(log, index)                                                                              \
	do {                                                                                                           \
		if (index > 2) {                                                                                       \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                             \
			                 "Invalid view index %d, only 2 views supported", index);                      \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_SWAPCHAIN_USAGE_FLAGS_NOT_MUTUALLY_EXCLUSIVE(log, flags, mutually_exclusive_a,                      \
                                                                mutually_exclusive_b)                                  \
	do {                                                                                                           \
		if (((flags) & (mutually_exclusive_a)) != 0 && ((flags) & (mutually_exclusive_b)) != 0) {              \
			return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                             \
			                 "(" #flags ") Swapchain usage flags " #mutually_exclusive_a                   \
			                 " and " #mutually_exclusive_b                                                 \
			                 " are mutually exclusive in this graphics API");                              \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_SESSION_NOT_LOST(log, sess)                                                                         \
	do {                                                                                                           \
		if (sess->has_lost) {                                                                                  \
			return oxr_error(log, XR_ERROR_SESSION_LOST, "Session is lost");                               \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_SESSION_RUNNING(log, sess)                                                                          \
	do {                                                                                                           \
		if (!sess->has_begun) {                                                                                \
			return oxr_error(log, XR_ERROR_SESSION_NOT_RUNNING, "Session is not running");                 \
		}                                                                                                      \
	} while (false)

#define OXR_VERIFY_PASSTHROUGH_FLAGS(log, flags)                                                                       \
	if (flags == 0 ||                                                                                              \
	    (flags & (XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB | XR_PASSTHROUGH_LAYER_DEPTH_BIT_FB)) == 0)         \
		return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                                     \
		                 "flags is not a valid combination of XrPassthroughFlagBitsFB values");

#define OXR_VERIFY_PASSTHROUGH_LAYER_PURPOSE(log, purpose)                                                             \
	if ((purpose != XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB &&                                              \
	     purpose != XR_PASSTHROUGH_LAYER_PURPOSE_PROJECTED_FB &&                                                   \
	     purpose != XR_PASSTHROUGH_LAYER_PURPOSE_TRACKED_KEYBOARD_HANDS_FB &&                                      \
	     purpose != XR_PASSTHROUGH_LAYER_PURPOSE_TRACKED_KEYBOARD_MASKED_HANDS_FB))                                \
		return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                                     \
		                 "purpose is not a valid XrPassthroughLayerPurposeFB value");

#define OXR_VERIFY_PASSTHROUGH_LAYER_STYLE(log, style)                                                                 \
	do {                                                                                                           \
		uint32_t duplicate_check = 0;                                                                          \
		const XrPassthroughStyleFB *next = style->next;                                                        \
		while (next) {                                                                                         \
			if (next->type != XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_RGBA_FB &&                             \
			    next->type != XR_TYPE_PASSTHROUGH_COLOR_MAP_MONO_TO_MONO_FB &&                             \
			    next->type != XR_TYPE_PASSTHROUGH_BRIGHTNESS_CONTRAST_SATURATION_FB)                       \
				return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                     \
				                 "style next structure chain contains invalid pointers");              \
			if ((next->type & duplicate_check) != 0)                                                       \
				return oxr_error(log, XR_ERROR_VALIDATION_FAILURE,                                     \
				                 "style next structure chain contains duplicate items");               \
			duplicate_check |= next->type;                                                                 \
			next = (const XrPassthroughStyleFB *)next->next;                                               \
		}                                                                                                      \
	} while (false)

/*
 *
 * Implementation in oxr_verify.cpp
 *
 */

XrResult
oxr_verify_full_path_c(struct oxr_logger *log, const char *path, const char *name);

/*!
 * Verify a full path.
 *
 * Length not including zero terminator character but must be there.
 */
XrResult
oxr_verify_full_path(struct oxr_logger *log, const char *path, size_t length, const char *name);

/*!
 * Verify a single path level that sits inside of a fixed sized array.
 */
XrResult
oxr_verify_fixed_size_single_level_path(struct oxr_logger * /*log*/,
                                        const char *path,
                                        uint32_t array_size,
                                        const char *name);

/*!
 * Verify an arbitrary UTF-8 string that sits inside of a fixed sized array.
 */
XrResult
oxr_verify_localized_name(struct oxr_logger * /*log*/, const char *string, uint32_t array_size, const char *name);

/*!
 * Verify a set of subaction paths for action creation.
 */
XrResult
oxr_verify_subaction_paths_create(struct oxr_logger *log,
                                  struct oxr_instance *inst,
                                  uint32_t countSubactionPaths,
                                  const XrPath *subactionPaths,
                                  const char *variable);

/*!
 * Verify a set of subaction paths for action sync.
 */
XrResult
oxr_verify_subaction_path_sync(struct oxr_logger *log,
                               const struct oxr_instance *inst,
                               const struct oxr_action_set *act_set,
                               XrPath path,
                               uint32_t index);

/*!
 * Verify a set of subaction paths for action state get.
 */
XrResult
oxr_verify_subaction_path_get(struct oxr_logger *log,
                              struct oxr_instance *inst,
                              XrPath path,
                              const struct oxr_subaction_paths *act_subaction_paths,
                              struct oxr_subaction_paths *out_subaction_paths,
                              const char *variable);

XrResult
oxr_verify_extensions(struct oxr_logger *log, const struct oxr_extension_status *extensions);

XrResult
oxr_verify_view_config_type(struct oxr_logger *log,
                            struct oxr_instance *inst,
                            XrViewConfigurationType view_conf,
                            const char *view_conf_name);

XrResult
oxr_verify_XrSessionCreateInfo(struct oxr_logger * /*log*/,
                               const struct oxr_instance * /*inst*/,
                               const XrSessionCreateInfo * /*createInfo*/);

#if defined(XR_USE_PLATFORM_XLIB) && defined(XR_USE_GRAPHICS_API_OPENGL)
XrResult
oxr_verify_XrGraphicsBindingOpenGLXlibKHR(struct oxr_logger * /*log*/, const XrGraphicsBindingOpenGLXlibKHR * /*next*/);
#endif // defined(XR_USE_PLATFORM_XLIB) && defined(XR_USE_GRAPHICS_API_OPENGL)

#if defined(XR_USE_PLATFORM_WIN32) && defined(XR_USE_GRAPHICS_API_OPENGL)
XrResult
oxr_verify_XrGraphicsBindingOpenGLWin32KHR(struct oxr_logger * /*log*/,
                                           const XrGraphicsBindingOpenGLWin32KHR * /*next*/);
#endif // defined(XR_USE_PLATFORM_WIN32) && defined(XR_USE_GRAPHICS_API_OPENGL)

#if defined(XR_USE_GRAPHICS_API_VULKAN)
XrResult
oxr_verify_XrGraphicsBindingVulkanKHR(struct oxr_logger * /*log*/, const XrGraphicsBindingVulkanKHR * /*next*/);
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)

#if defined(XR_USE_PLATFORM_EGL) && defined(XR_USE_GRAPHICS_API_OPENGL)
XrResult
oxr_verify_XrGraphicsBindingEGLMNDX(struct oxr_logger *log, const XrGraphicsBindingEGLMNDX *next);
#endif // defined(XR_USE_PLATFORM_EGL) && defined(XR_USE_GRAPHICS_API_OPENGL)

#if defined(XR_USE_PLATFORM_ANDROID) && defined(XR_USE_GRAPHICS_API_OPENGL_ES)
XrResult
oxr_verify_XrGraphicsBindingOpenGLESAndroidKHR(struct oxr_logger *, const XrGraphicsBindingOpenGLESAndroidKHR *);
#endif // defined(XR_USE_PLATFORM_ANDROID) &&
       // defined(XR_USE_GRAPHICS_API_OPENGL_ES)

#if defined(XR_USE_GRAPHICS_API_D3D11)
XrResult
oxr_verify_XrGraphicsBindingD3D11KHR(struct oxr_logger *, const XrGraphicsBindingD3D11KHR *);
#endif // defined(XR_USE_GRAPHICS_API_D3D11)

#if defined(XR_USE_GRAPHICS_API_D3D12)
XrResult
oxr_verify_XrGraphicsBindingD3D12KHR(struct oxr_logger *, const XrGraphicsBindingD3D12KHR *);
#endif // defined(XR_USE_GRAPHICS_API_D3D12)

#ifdef XR_EXT_dpad_binding
XrResult
oxr_verify_XrInteractionProfileDpadBindingEXT(struct oxr_logger *,
                                              const XrInteractionProfileDpadBindingEXT *,
                                              const char *error_prefix);
#endif // XR_EXT_dpad_binding

/*!
 * @}
 */


#ifdef __cplusplus
}
#endif

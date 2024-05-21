// Copyright 2018-2024, Collabora, Ltd.
// Copyright 2023, NVIDIA CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  The objects representing OpenXR handles, and prototypes for internal functions used in the state tracker.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup oxr_main
 */

#pragma once

#include "xrt/xrt_space.h"
#include "xrt/xrt_limits.h"
#include "xrt/xrt_system.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_tracking.h"
#include "xrt/xrt_compositor.h"
#include "xrt/xrt_vulkan_includes.h"
#include "xrt/xrt_openxr_includes.h"
#include "xrt/xrt_config_os.h"
#include "xrt/xrt_config_have.h"

#include "os/os_threading.h"

#include "util/u_index_fifo.h"
#include "util/u_hashset.h"
#include "util/u_hashmap.h"
#include "util/u_device.h"

#include "oxr_extension_support.h"
#include "oxr_subaction.h"
#include "oxr_defines.h"

#if defined(XRT_HAVE_D3D11) || defined(XRT_HAVE_D3D12)
#include <dxgi.h>
#include <d3dcommon.h>
#endif

#ifdef XRT_FEATURE_RENDERDOC
#include "renderdoc_app.h"
#ifndef XRT_OS_WINDOWS
#include <dlfcn.h>
#endif // !XRT_OS_WINDOWS
#endif // XRT_FEATURE_RENDERDOC

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup oxr OpenXR state tracker
 *
 * Client application facing code.
 *
 * @ingroup xrt
 */

/*!
 * @brief Cast a pointer to an OpenXR handle in such a way as to avoid warnings.
 *
 * Avoids -Wpointer-to-int-cast by first casting to the same size int, then
 * promoting to the 64-bit int, then casting to the handle type. That's a lot of
 * no-ops on 64-bit, but a widening conversion on 32-bit.
 *
 * @ingroup oxr
 */
#define XRT_CAST_PTR_TO_OXR_HANDLE(HANDLE_TYPE, PTR) ((HANDLE_TYPE)(uint64_t)(uintptr_t)(PTR))

/*!
 * @brief Cast an OpenXR handle to a pointer in such a way as to avoid warnings.
 *
 * Avoids -Wint-to-pointer-cast by first casting to a 64-bit int, then to a
 * pointer-sized int, then to the desired pointer type. That's a lot of no-ops
 * on 64-bit, but a narrowing (!) conversion on 32-bit.
 *
 * @ingroup oxr
 */
#define XRT_CAST_OXR_HANDLE_TO_PTR(PTR_TYPE, HANDLE) ((PTR_TYPE)(uintptr_t)(uint64_t)(HANDLE))

/*!
 * @defgroup oxr_main OpenXR main code
 *
 * Gets called from @ref oxr_api functions and talks to devices and
 * @ref comp using @ref xrt_iface.
 *
 * @ingroup oxr
 * @{
 */


/*
 *
 * Forward declare structs.
 *
 */

struct xrt_instance;
struct oxr_logger;
struct oxr_extension_status;
struct oxr_instance;
struct oxr_system;
struct oxr_session;
struct oxr_event;
struct oxr_swapchain;
struct oxr_space;
struct oxr_action_set;
struct oxr_action;
struct oxr_debug_messenger;
struct oxr_handle_base;
struct oxr_subaction_paths;
struct oxr_action_attachment;
struct oxr_action_set_attachment;
struct oxr_action_input;
struct oxr_action_output;
struct oxr_dpad_state;
struct oxr_binding;
struct oxr_interaction_profile;
struct oxr_action_set_ref;
struct oxr_action_ref;
struct oxr_hand_tracker;
struct oxr_facial_tracker_htc;
struct oxr_facial_tracker_fb;
struct oxr_body_tracker_fb;
struct oxr_xdev_list;

#define XRT_MAX_HANDLE_CHILDREN 256
#define OXR_MAX_BINDINGS_PER_ACTION 32

struct time_state;

/*!
 * Function pointer type for a handle destruction function.
 *
 * @relates oxr_handle_base
 */
typedef XrResult (*oxr_handle_destroyer)(struct oxr_logger *log, struct oxr_handle_base *hb);



/*
 *
 * Helpers
 *
 */

/*!
 * Safely copy a xrt_pose to a XrPosef.
 */
#define OXR_XRT_POSE_TO_XRPOSEF(FROM, TO)                                                                              \
	do {                                                                                                           \
		union {                                                                                                \
			struct xrt_pose xrt;                                                                           \
			XrPosef oxr;                                                                                   \
		} safe_copy = {FROM};                                                                                  \
		TO = safe_copy.oxr;                                                                                    \
	} while (false)

/*!
 * Safely copy a xrt_fov to a XrFovf.
 */
#define OXR_XRT_FOV_TO_XRFOVF(FROM, TO)                                                                                \
	do {                                                                                                           \
		union {                                                                                                \
			struct xrt_fov xrt;                                                                            \
			XrFovf oxr;                                                                                    \
		} safe_copy = {FROM};                                                                                  \
		TO = safe_copy.oxr;                                                                                    \
	} while (false)


/*
 *
 * oxr_handle_base.c
 *
 */

/*!
 * Destroy the handle's object, as well as all child handles recursively.
 *
 * This should be how all handle-associated objects are destroyed.
 *
 * @public @memberof oxr_handle_base
 */
XrResult
oxr_handle_destroy(struct oxr_logger *log, struct oxr_handle_base *hb);

/*!
 * Returns a human-readable label for a handle state.
 *
 * @relates oxr_handle_base
 */
const char *
oxr_handle_state_to_string(enum oxr_handle_state state);

/*!
 *
 * @name oxr_instance.c
 * @{
 *
 */

/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_instance
 */
static inline XrInstance
oxr_instance_to_openxr(struct oxr_instance *inst)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrInstance, inst);
}

/*!
 * Creates a instance, does minimal validation of @p createInfo.
 *
 * @param[in]  log        Logger
 * @param[in]  createInfo OpenXR creation info.
 * @param[in]  extensions Parsed extension list to be enabled.
 * @param[out] out_inst   Pointer to pointer to a instance, returned instance.
 *
 * @public @static @memberof oxr_instance
 */
XrResult
oxr_instance_create(struct oxr_logger *log,
                    const XrInstanceCreateInfo *createInfo,
                    const struct oxr_extension_status *extensions,
                    struct oxr_instance **out_inst);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_instance_get_properties(struct oxr_logger *log,
                            struct oxr_instance *inst,
                            XrInstanceProperties *instanceProperties);

#if XR_USE_TIMESPEC

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_instance_convert_time_to_timespec(struct oxr_logger *log,
                                      struct oxr_instance *inst,
                                      XrTime time,
                                      struct timespec *timespecTime);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_instance_convert_timespec_to_time(struct oxr_logger *log,
                                      struct oxr_instance *inst,
                                      const struct timespec *timespecTime,
                                      XrTime *time);
#endif // XR_USE_TIMESPEC

#ifdef XR_USE_PLATFORM_WIN32

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_instance_convert_time_to_win32perfcounter(struct oxr_logger *log,
                                              struct oxr_instance *inst,
                                              XrTime time,
                                              LARGE_INTEGER *win32perfcounterTime);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_instance_convert_win32perfcounter_to_time(struct oxr_logger *log,
                                              struct oxr_instance *inst,
                                              const LARGE_INTEGER *win32perfcounterTime,
                                              XrTime *time);

#endif // XR_USE_PLATFORM_WIN32

/*!
 * @}
 */

/*!
 *
 * @name oxr_path.c
 * @{
 *
 */

/*!
 * Initialize the path system.
 * @private @memberof oxr_instance
 */
XrResult
oxr_path_init(struct oxr_logger *log, struct oxr_instance *inst);

/*!
 * @public @memberof oxr_instance
 */
bool
oxr_path_is_valid(struct oxr_logger *log, struct oxr_instance *inst, XrPath path);

/*!
 * @public @memberof oxr_instance
 */
void *
oxr_path_get_attached(struct oxr_logger *log, struct oxr_instance *inst, XrPath path);

/*!
 * Get the path for the given string if it exists, or create it if it does not.
 *
 * @public @memberof oxr_instance
 */
XrResult
oxr_path_get_or_create(
    struct oxr_logger *log, struct oxr_instance *inst, const char *str, size_t length, XrPath *out_path);

/*!
 * Only get the path for the given string if it exists.
 *
 * @public @memberof oxr_instance
 */
XrResult
oxr_path_only_get(struct oxr_logger *log, struct oxr_instance *inst, const char *str, size_t length, XrPath *out_path);

/*!
 * Get a pointer and length of the internal string.
 *
 * The pointer has the same life time as the instance. The length is the number
 * of valid characters, not including the null termination character (but an
 * extra null byte is always reserved at the end so can strings can be given
 * to functions expecting null terminated strings).
 *
 * @public @memberof oxr_instance
 */
XrResult
oxr_path_get_string(
    struct oxr_logger *log, const struct oxr_instance *inst, XrPath path, const char **out_str, size_t *out_length);

/*!
 * Destroy the path system and all paths that the instance has created.
 *
 * @private @memberof oxr_instance
 */
void
oxr_path_destroy(struct oxr_logger *log, struct oxr_instance *inst);

/*!
 * @}
 */

/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_action_set
 */
static inline XrActionSet
oxr_action_set_to_openxr(struct oxr_action_set *act_set)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrActionSet, act_set);
}

/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_hand_tracker
 */
static inline XrHandTrackerEXT
oxr_hand_tracker_to_openxr(struct oxr_hand_tracker *hand_tracker)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrHandTrackerEXT, hand_tracker);
}

/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_action
 */
static inline XrAction
oxr_action_to_openxr(struct oxr_action *act)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrAction, act);
}

#ifdef OXR_HAVE_HTC_facial_tracking
/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_facial_tracker_htc
 */
static inline XrFacialTrackerHTC
oxr_facial_tracker_htc_to_openxr(struct oxr_facial_tracker_htc *face_tracker_htc)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrFacialTrackerHTC, face_tracker_htc);
}
#endif

#ifdef OXR_HAVE_FB_body_tracking
/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_facial_tracker_htc
 */
static inline XrBodyTrackerFB
oxr_body_tracker_fb_to_openxr(struct oxr_body_tracker_fb *body_tracker_fb)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrBodyTrackerFB, body_tracker_fb);
}
#endif

/*!
 *
 * @name oxr_input.c
 * @{
 *
 */

/*!
 * Helper function to classify subaction_paths.
 *
 * Sets all members of @p subaction_paths ( @ref oxr_subaction_paths ) as
 * appropriate based on the subaction paths found in the list.
 *
 * If no paths are provided, @p subaction_paths->any will be true.
 *
 * @return false if an invalid subaction path is provided.
 *
 * @public @memberof oxr_instance
 * @see oxr_subaction_paths
 */
bool
oxr_classify_subaction_paths(struct oxr_logger *log,
                             const struct oxr_instance *inst,
                             uint32_t subaction_path_count,
                             const XrPath *subaction_paths,
                             struct oxr_subaction_paths *subaction_paths_out);

/*!
 * Find the pose input for the set of subaction_paths
 *
 * @public @memberof oxr_session
 */
XrResult
oxr_action_get_pose_input(struct oxr_session *sess,
                          uint32_t act_key,
                          const struct oxr_subaction_paths *subaction_paths_ptr,
                          struct oxr_action_input **out_input);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_action_set_create(struct oxr_logger *log,
                      struct oxr_instance *inst,
                      const XrActionSetCreateInfo *createInfo,
                      struct oxr_action_set **out_act_set);

/*!
 * @public @memberof oxr_action
 */
XrResult
oxr_action_create(struct oxr_logger *log,
                  struct oxr_action_set *act_set,
                  const XrActionCreateInfo *createInfo,
                  struct oxr_action **out_act);

/*!
 * @public @memberof oxr_session
 * @see oxr_action_set
 */
XrResult
oxr_session_attach_action_sets(struct oxr_logger *log,
                               struct oxr_session *sess,
                               const XrSessionActionSetsAttachInfo *bindInfo);


XrResult
oxr_session_update_action_bindings(struct oxr_logger *log, struct oxr_session *sess);

/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_sync_data(struct oxr_logger *log,
                     struct oxr_session *sess,
                     uint32_t countActionSets,
                     const XrActiveActionSet *actionSets);

/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_enumerate_bound_sources(struct oxr_logger *log,
                                   struct oxr_session *sess,
                                   uint32_t act_key,
                                   uint32_t sourceCapacityInput,
                                   uint32_t *sourceCountOutput,
                                   XrPath *sources);

/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_get_boolean(struct oxr_logger *log,
                       struct oxr_session *sess,
                       uint32_t act_key,
                       struct oxr_subaction_paths subaction_paths,
                       XrActionStateBoolean *data);

/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_get_vector1f(struct oxr_logger *log,
                        struct oxr_session *sess,
                        uint32_t act_key,
                        struct oxr_subaction_paths subaction_paths,
                        XrActionStateFloat *data);

/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_get_vector2f(struct oxr_logger *log,
                        struct oxr_session *sess,
                        uint32_t act_key,
                        struct oxr_subaction_paths subaction_paths,
                        XrActionStateVector2f *data);
/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_get_pose(struct oxr_logger *log,
                    struct oxr_session *sess,
                    uint32_t act_key,
                    struct oxr_subaction_paths subaction_paths,
                    XrActionStatePose *data);
/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_apply_haptic_feedback(struct oxr_logger *log,
                                 struct oxr_session *sess,
                                 uint32_t act_key,
                                 struct oxr_subaction_paths subaction_paths,
                                 const XrHapticBaseHeader *hapticEvent);
/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_stop_haptic_feedback(struct oxr_logger *log,
                                struct oxr_session *sess,
                                uint32_t act_key,
                                struct oxr_subaction_paths subaction_paths);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_hand_tracker_create(struct oxr_logger *log,
                        struct oxr_session *sess,
                        const XrHandTrackerCreateInfoEXT *createInfo,
                        struct oxr_hand_tracker **out_hand_tracker);

/*!
 * @}
 */

/*!
 *
 * @name oxr_binding.c
 * @{
 *
 */

/*!
 * Find the best matching profile for the given @ref xrt_device.
 *
 * @param      log   Logger.
 * @param      sess  Session.
 * @param      xdev  Can be null.
 * @param[out] out_p Returned interaction profile.
 *
 * @public @memberof oxr_session
 */
void
oxr_find_profile_for_device(struct oxr_logger *log,
                            struct oxr_session *sess,
                            struct xrt_device *xdev,
                            struct oxr_interaction_profile **out_p);

void
oxr_get_profile_for_device_name(struct oxr_logger *log,
                                struct oxr_session *sess,
                                enum xrt_device_name name,
                                struct oxr_interaction_profile **out_p);

struct oxr_interaction_profile *
oxr_clone_profile(const struct oxr_interaction_profile *src_profile);

/*!
 * Free all memory allocated by the binding system.
 *
 * @public @memberof oxr_instance
 */
void
oxr_binding_destroy_all(struct oxr_logger *log, struct oxr_instance *inst);

/*!
 * Free all memory allocated by the binding system.
 *
 * @public @memberof oxr_instance
 */
void
oxr_session_binding_destroy_all(struct oxr_logger *log, struct oxr_session *sess);

/*!
 * Find all bindings that is the given action key is bound to.
 * @public @memberof oxr_interaction_profile
 */
void
oxr_binding_find_bindings_from_key(struct oxr_logger *log,
                                   struct oxr_interaction_profile *p,
                                   uint32_t key,
                                   size_t max_bounding_count,
                                   struct oxr_binding **bindings,
                                   size_t *out_binding_count);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_action_suggest_interaction_profile_bindings(struct oxr_logger *log,
                                                struct oxr_instance *inst,
                                                const XrInteractionProfileSuggestedBinding *suggestedBindings,
                                                struct oxr_dpad_state *state);

/*!
 * @public @memberof oxr_instance
 */
XrResult
oxr_action_get_current_interaction_profile(struct oxr_logger *log,
                                           struct oxr_session *sess,
                                           XrPath topLevelUserPath,
                                           XrInteractionProfileState *interactionProfile);

/*!
 * @public @memberof oxr_session
 */
XrResult
oxr_action_get_input_source_localized_name(struct oxr_logger *log,
                                           struct oxr_session *sess,
                                           const XrInputSourceLocalizedNameGetInfo *getInfo,
                                           uint32_t bufferCapacityInput,
                                           uint32_t *bufferCountOutput,
                                           char *buffer);

/*!
 * @}
 */


/*!
 *
 * @name oxr_dpad.c
 * @{
 *
 */

/*!
 * Initialises a dpad state, has to be zero init before a call to this function.
 *
 * @public @memberof oxr_dpad_state_get
 */
bool
oxr_dpad_state_init(struct oxr_dpad_state *state);

/*!
 * Look for a entry in the state for the given action set key,
 * returns NULL if no entry has been made for that action set.
 *
 * @public @memberof oxr_dpad_state_get
 */
struct oxr_dpad_entry *
oxr_dpad_state_get(struct oxr_dpad_state *state, uint64_t key);

/*!
 * Look for a entry in the state for the given action set key,
 * allocates a new entry if none was found.
 *
 * @public @memberof oxr_dpad_state_get
 */
struct oxr_dpad_entry *
oxr_dpad_state_get_or_add(struct oxr_dpad_state *state, uint64_t key);

/*!
 * Frees all state and entries attached to this dpad state.
 *
 * @public @memberof oxr_dpad_state_get
 */
void
oxr_dpad_state_deinit(struct oxr_dpad_state *state);

/*!
 * Clones all oxr_dpad_state
 * @param dst_dpad_state destination of cloning
 * @param src_dpad_state source of cloning
 * @public @memberof oxr_dpad_state_clone
 */
bool
oxr_dpad_state_clone(struct oxr_dpad_state *dst_dpad_state, const struct oxr_dpad_state *src_dpad_state);


/*!
 * @}
 */


/*!
 *
 * @name oxr_session.c
 * @{
 *
 */

/*!
 * To go back to a OpenXR object.
 *
 * @relates oxr_session
 */
static inline XrSession
oxr_session_to_openxr(struct oxr_session *sess)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrSession, sess);
}

XrResult
oxr_session_create(struct oxr_logger *log,
                   struct oxr_system *sys,
                   const XrSessionCreateInfo *createInfo,
                   struct oxr_session **out_session);

XrResult
oxr_session_enumerate_formats(struct oxr_logger *log,
                              struct oxr_session *sess,
                              uint32_t formatCapacityInput,
                              uint32_t *formatCountOutput,
                              int64_t *formats);

/*!
 * Change the state of the session, queues a event.
 */
void
oxr_session_change_state(struct oxr_logger *log, struct oxr_session *sess, XrSessionState state, XrTime time);

XrResult
oxr_session_begin(struct oxr_logger *log, struct oxr_session *sess, const XrSessionBeginInfo *beginInfo);

XrResult
oxr_session_end(struct oxr_logger *log, struct oxr_session *sess);

XrResult
oxr_session_request_exit(struct oxr_logger *log, struct oxr_session *sess);

XRT_CHECK_RESULT XrResult
oxr_session_poll(struct oxr_logger *log, struct oxr_session *sess);

XrResult
oxr_session_locate_views(struct oxr_logger *log,
                         struct oxr_session *sess,
                         const XrViewLocateInfo *viewLocateInfo,
                         XrViewState *viewState,
                         uint32_t viewCapacityInput,
                         uint32_t *viewCountOutput,
                         XrView *views);

XrResult
oxr_session_frame_wait(struct oxr_logger *log, struct oxr_session *sess, XrFrameState *frameState);

XrResult
oxr_session_frame_begin(struct oxr_logger *log, struct oxr_session *sess);

XrResult
oxr_session_frame_end(struct oxr_logger *log, struct oxr_session *sess, const XrFrameEndInfo *frameEndInfo);

XrResult
oxr_session_hand_joints(struct oxr_logger *log,
                        struct oxr_hand_tracker *hand_tracker,
                        const XrHandJointsLocateInfoEXT *locateInfo,
                        XrHandJointLocationsEXT *locations);

/*
 * Gets the body pose in the base space.
 */
XrResult
oxr_get_base_body_pose(struct oxr_logger *log,
                       const struct xrt_body_joint_set *body_joint_set,
                       struct oxr_space *base_spc,
                       struct xrt_device *body_xdev,
                       XrTime at_time,
                       struct xrt_space_relation *out_base_body);

XrResult
oxr_session_apply_force_feedback(struct oxr_logger *log,
                                 struct oxr_hand_tracker *hand_tracker,
                                 const XrForceFeedbackCurlApplyLocationsMNDX *locations);

#ifdef OXR_HAVE_KHR_android_thread_settings
XrResult
oxr_session_android_thread_settings(struct oxr_logger *log,
                                    struct oxr_session *sess,
                                    XrAndroidThreadTypeKHR threadType,
                                    uint32_t threadId);
#endif // OXR_HAVE_KHR_android_thread_settings

#ifdef OXR_HAVE_KHR_visibility_mask
XrResult
oxr_session_get_visibility_mask(struct oxr_logger *log,
                                struct oxr_session *session,
                                XrVisibilityMaskTypeKHR visibilityMaskType,
                                uint32_t viewIndex,
                                XrVisibilityMaskKHR *visibilityMask);

XrResult
oxr_event_push_XrEventDataVisibilityMaskChangedKHR(struct oxr_logger *log,
                                                   struct oxr_session *sess,
                                                   XrViewConfigurationType viewConfigurationType,
                                                   uint32_t viewIndex);
#endif // OXR_HAVE_KHR_visibility_mask

#ifdef OXR_HAVE_EXT_performance_settings
XrResult
oxr_session_set_perf_level(struct oxr_logger *log,
                           struct oxr_session *sess,
                           XrPerfSettingsDomainEXT domain,
                           XrPerfSettingsLevelEXT level);
#endif // OXR_HAVE_EXT_performance_settings

/*
 *
 * oxr_space.c
 *
 */

/*!
 * To go back to a OpenXR object.
 */
static inline XrSpace
oxr_space_to_openxr(struct oxr_space *spc)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrSpace, spc);
}

XrResult
oxr_space_action_create(struct oxr_logger *log,
                        struct oxr_session *sess,
                        uint32_t key,
                        const XrActionSpaceCreateInfo *createInfo,
                        struct oxr_space **out_space);

XrResult
oxr_space_get_reference_bounds_rect(struct oxr_logger *log,
                                    struct oxr_session *sess,
                                    XrReferenceSpaceType referenceSpaceType,
                                    XrExtent2Df *bounds);

XrResult
oxr_space_reference_create(struct oxr_logger *log,
                           struct oxr_session *sess,
                           const XrReferenceSpaceCreateInfo *createInfo,
                           struct oxr_space **out_space);

/*!
 * Monado special space that always points to a specific @ref xrt_device and
 * pose, useful when you want to bypass the action binding system for instance.
 */
XrResult
oxr_space_xdev_pose_create(struct oxr_logger *log,
                           struct oxr_session *sess,
                           struct xrt_device *xdev,
                           enum xrt_input_name name,
                           const struct xrt_pose *pose,
                           struct oxr_space **out_space);

XrResult
oxr_space_locate(
    struct oxr_logger *log, struct oxr_space *spc, struct oxr_space *baseSpc, XrTime time, XrSpaceLocation *location);

/*!
 * Locate the @ref xrt_device in the given base space, useful for implementing
 * hand tracking location look ups and the like.
 *
 * @param      log          Logging struct.
 * @param      xdev         Device to locate in the base space.
 * @param      baseSpc      Base space where the device is to be located.
 * @param[in]  time         Time in OpenXR domain.
 * @param[out] out_relation Returns T_base_xdev, aka xdev in base space.
 *
 * @return Any errors, XR_SUCCESS, pose might not be valid on XR_SUCCESS.
 */
XRT_CHECK_RESULT XrResult
oxr_space_locate_device(struct oxr_logger *log,
                        struct xrt_device *xdev,
                        struct oxr_space *baseSpc,
                        XrTime time,
                        struct xrt_space_relation *out_relation);


/*
 *
 * oxr_swapchain.c
 *
 */

/*!
 * To go back to a OpenXR object.
 */
static inline XrSwapchain
oxr_swapchain_to_openxr(struct oxr_swapchain *sc)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrSwapchain, sc);
}


/*
 *
 * oxr_messenger.c
 *
 */

/*!
 * To go back to a OpenXR object.
 */
static inline XrDebugUtilsMessengerEXT
oxr_messenger_to_openxr(struct oxr_debug_messenger *mssngr)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrDebugUtilsMessengerEXT, mssngr);
}

XrResult
oxr_create_messenger(struct oxr_logger *,
                     struct oxr_instance *inst,
                     const XrDebugUtilsMessengerCreateInfoEXT *,
                     struct oxr_debug_messenger **out_mssngr);
XrResult
oxr_destroy_messenger(struct oxr_logger *log, struct oxr_debug_messenger *mssngr);


/*
 *
 * oxr_system.c
 *
 */

XrResult
oxr_system_select(struct oxr_logger *log,
                  struct oxr_system **systems,
                  uint32_t system_count,
                  XrFormFactor form_factor,
                  struct oxr_system **out_selected);

XrResult
oxr_system_fill_in(struct oxr_logger *log,
                   struct oxr_instance *inst,
                   XrSystemId systemId,
                   uint32_t view_count,
                   struct oxr_system *sys);

XrResult
oxr_system_verify_id(struct oxr_logger *log, const struct oxr_instance *inst, XrSystemId systemId);

XrResult
oxr_system_get_by_id(struct oxr_logger *log,
                     struct oxr_instance *inst,
                     XrSystemId systemId,
                     struct oxr_system **system);

XrResult
oxr_system_get_properties(struct oxr_logger *log, struct oxr_system *sys, XrSystemProperties *properties);

XrResult
oxr_system_enumerate_view_confs(struct oxr_logger *log,
                                struct oxr_system *sys,
                                uint32_t viewConfigurationTypeCapacityInput,
                                uint32_t *viewConfigurationTypeCountOutput,
                                XrViewConfigurationType *viewConfigurationTypes);

XrResult
oxr_system_enumerate_blend_modes(struct oxr_logger *log,
                                 struct oxr_system *sys,
                                 XrViewConfigurationType viewConfigurationType,
                                 uint32_t environmentBlendModeCapacityInput,
                                 uint32_t *environmentBlendModeCountOutput,
                                 XrEnvironmentBlendMode *environmentBlendModes);

XrResult
oxr_system_get_view_conf_properties(struct oxr_logger *log,
                                    struct oxr_system *sys,
                                    XrViewConfigurationType viewConfigurationType,
                                    XrViewConfigurationProperties *configurationProperties);

XrResult
oxr_system_enumerate_view_conf_views(struct oxr_logger *log,
                                     struct oxr_system *sys,
                                     XrViewConfigurationType viewConfigurationType,
                                     uint32_t viewCapacityInput,
                                     uint32_t *viewCountOutput,
                                     XrViewConfigurationView *views);

bool
oxr_system_get_hand_tracking_support(struct oxr_logger *log, struct oxr_instance *inst);

bool
oxr_system_get_eye_gaze_support(struct oxr_logger *log, struct oxr_instance *inst);

bool
oxr_system_get_force_feedback_support(struct oxr_logger *log, struct oxr_instance *inst);

void
oxr_system_get_face_tracking_htc_support(struct oxr_logger *log,
                                         struct oxr_instance *inst,
                                         bool *supports_eye,
                                         bool *supports_lip);

bool
oxr_system_get_body_tracking_fb_support(struct oxr_logger *log, struct oxr_instance *inst);

/*
 *
 * oxr_event.cpp
 *
 */

XrResult
oxr_event_push_XrEventDataSessionStateChanged(struct oxr_logger *log,
                                              struct oxr_session *sess,
                                              XrSessionState state,
                                              XrTime time);

XrResult
oxr_event_push_XrEventDataInteractionProfileChanged(struct oxr_logger *log, struct oxr_session *sess);

XrResult
oxr_event_push_XrEventDataReferenceSpaceChangePending(struct oxr_logger *log,
                                                      struct oxr_session *sess,
                                                      XrReferenceSpaceType referenceSpaceType,
                                                      XrTime changeTime,
                                                      XrBool32 poseValid,
                                                      const XrPosef *poseInPreviousSpace);

#ifdef OXR_HAVE_FB_display_refresh_rate
XrResult
oxr_event_push_XrEventDataDisplayRefreshRateChangedFB(struct oxr_logger *log,
                                                      struct oxr_session *sess,
                                                      float fromDisplayRefreshRate,
                                                      float toDisplayRefreshRate);
#endif // OXR_HAVE_FB_display_refresh_rate

#ifdef OXR_HAVE_EXTX_overlay
XrResult
oxr_event_push_XrEventDataMainSessionVisibilityChangedEXTX(struct oxr_logger *log,
                                                           struct oxr_session *sess,
                                                           bool visible);
#endif // OXR_HAVE_EXTX_overlay

#ifdef OXR_HAVE_EXT_performance_settings
XrResult
oxr_event_push_XrEventDataPerfSettingsEXTX(struct oxr_logger *log,
                                           struct oxr_session *sess,
                                           enum xrt_perf_domain domain,
                                           enum xrt_perf_sub_domain subDomain,
                                           enum xrt_perf_notify_level fromLevel,
                                           enum xrt_perf_notify_level toLevel);
#endif // OXR_HAVE_EXT_performance_settings
/*!
 * This clears all pending events refers to the given session.
 */
XrResult
oxr_event_remove_session_events(struct oxr_logger *log, struct oxr_session *sess);

/*!
 * Will return one event if available, also drain the sessions event queues.
 */
XrResult
oxr_poll_event(struct oxr_logger *log, struct oxr_instance *inst, XrEventDataBuffer *eventData);


/*
 *
 * oxr_xdev.c
 *
 */

void
oxr_xdev_destroy(struct xrt_device **xdev_ptr);

void
oxr_xdev_update(struct xrt_device *xdev);

/*!
 * Return true if it finds an input of that name on this device.
 */
bool
oxr_xdev_find_input(struct xrt_device *xdev, enum xrt_input_name name, struct xrt_input **out_input);

/*!
 * Return true if it finds an output of that name on this device.
 */
bool
oxr_xdev_find_output(struct xrt_device *xdev, enum xrt_output_name name, struct xrt_output **out_output);

/*!
 * Returns the hand tracking value of the named input from the device.
 * Does NOT apply tracking origin offset to each joint.
 */
void
oxr_xdev_get_hand_tracking_at(struct oxr_logger *log,
                              struct oxr_instance *inst,
                              struct xrt_device *xdev,
                              enum xrt_input_name name,
                              XrTime at_time,
                              struct xrt_hand_joint_set *out_value);

#ifdef OXR_HAVE_MNDX_xdev_space
static inline XrXDevListMNDX
oxr_xdev_list_to_openxr(struct oxr_xdev_list *sc)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrXDevListMNDX, sc);
}

XrResult
oxr_xdev_list_create(struct oxr_logger *log,
                     struct oxr_session *sess,
                     const XrCreateXDevListInfoMNDX *createInfo,
                     struct oxr_xdev_list **out_xdl);

XrResult
oxr_xdev_list_get_properties(struct oxr_logger *log,
                             struct oxr_xdev_list *xdl,
                             uint32_t index,
                             XrXDevPropertiesMNDX *properties);

XrResult
oxr_xdev_list_space_create(struct oxr_logger *log,
                           struct oxr_xdev_list *xdl,
                           const XrCreateXDevSpaceInfoMNDX *createInfo,
                           uint32_t index,
                           struct oxr_space **out_space);

#endif // OXR_HAVE_MNDX_xdev_space


/*
 *
 * OpenGL, located in various files.
 *
 */

#ifdef XR_USE_GRAPHICS_API_OPENGL
#ifdef XR_USE_PLATFORM_XLIB

XrResult
oxr_session_populate_gl_xlib(struct oxr_logger *log,
                             struct oxr_system *sys,
                             XrGraphicsBindingOpenGLXlibKHR const *next,
                             struct oxr_session *sess);
#endif // XR_USE_PLATFORM_XLIB

#ifdef XR_USE_PLATFORM_WIN32
XrResult
oxr_session_populate_gl_win32(struct oxr_logger *log,
                              struct oxr_system *sys,
                              XrGraphicsBindingOpenGLWin32KHR const *next,
                              struct oxr_session *sess);
#endif // XR_USE_PLATFORM_WIN32
#endif // XR_USE_GRAPHICS_API_OPENGL

#if defined(XR_USE_GRAPHICS_API_OPENGL) || defined(XR_USE_GRAPHICS_API_OPENGL_ES)
XrResult
oxr_swapchain_gl_create(struct oxr_logger * /*log*/,
                        struct oxr_session *sess,
                        const XrSwapchainCreateInfo * /*createInfo*/,
                        struct oxr_swapchain **out_swapchain);

#endif // XR_USE_GRAPHICS_API_OPENGL || XR_USE_GRAPHICS_API_OPENGL_ES

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
#if defined(XR_USE_PLATFORM_ANDROID)
XrResult
oxr_session_populate_gles_android(struct oxr_logger *log,
                                  struct oxr_system *sys,
                                  XrGraphicsBindingOpenGLESAndroidKHR const *next,
                                  struct oxr_session *sess);
#endif // XR_USE_PLATFORM_ANDROID
#endif // XR_USE_GRAPHICS_API_OPENGL_ES


/*
 *
 * Vulkan, located in various files.
 *
 */

#ifdef XR_USE_GRAPHICS_API_VULKAN

XrResult
oxr_vk_get_instance_exts(struct oxr_logger *log,
                         struct oxr_system *sys,
                         uint32_t namesCapacityInput,
                         uint32_t *namesCountOutput,
                         char *namesString);

XrResult
oxr_vk_get_device_exts(struct oxr_logger *log,
                       struct oxr_system *sys,
                       uint32_t namesCapacityInput,
                       uint32_t *namesCountOutput,
                       char *namesString);

XrResult
oxr_vk_get_requirements(struct oxr_logger *log,
                        struct oxr_system *sys,
                        XrGraphicsRequirementsVulkanKHR *graphicsRequirements);

XrResult
oxr_vk_create_vulkan_instance(struct oxr_logger *log,
                              struct oxr_system *sys,
                              const XrVulkanInstanceCreateInfoKHR *createInfo,
                              VkInstance *vulkanInstance,
                              VkResult *vulkanResult);

XrResult
oxr_vk_create_vulkan_device(struct oxr_logger *log,
                            struct oxr_system *sys,
                            const XrVulkanDeviceCreateInfoKHR *createInfo,
                            VkDevice *vulkanDevice,
                            VkResult *vulkanResult);

XrResult
oxr_vk_get_physical_device(struct oxr_logger *log,
                           struct oxr_instance *inst,
                           struct oxr_system *sys,
                           VkInstance vkInstance,
                           PFN_vkGetInstanceProcAddr getProc,
                           VkPhysicalDevice *vkPhysicalDevice);

XrResult
oxr_session_populate_vk(struct oxr_logger *log,
                        struct oxr_system *sys,
                        XrGraphicsBindingVulkanKHR const *next,
                        struct oxr_session *sess);

XrResult
oxr_swapchain_vk_create(struct oxr_logger * /*log*/,
                        struct oxr_session *sess,
                        const XrSwapchainCreateInfo * /*createInfo*/,
                        struct oxr_swapchain **out_swapchain);

#endif


/*
 *
 * EGL, located in various files.
 *
 */

#ifdef XR_USE_PLATFORM_EGL

XrResult
oxr_session_populate_egl(struct oxr_logger *log,
                         struct oxr_system *sys,
                         XrGraphicsBindingEGLMNDX const *next,
                         struct oxr_session *sess);

#endif

/*
 *
 * D3D version independent routines, located in oxr_d3d.cpp
 *
 */

#if defined(XRT_HAVE_D3D11) || defined(XRT_HAVE_D3D12) || defined(XRT_DOXYGEN)
/// Common GetRequirements call for D3D11 and D3D12
XrResult
oxr_d3d_get_requirements(struct oxr_logger *log,
                         struct oxr_system *sys,
                         LUID *adapter_luid,
                         D3D_FEATURE_LEVEL *min_feature_level);

/// Verify the provided LUID matches the expected one in @p sys
XrResult
oxr_d3d_check_luid(struct oxr_logger *log, struct oxr_system *sys, LUID *adapter_luid);
#endif

/*
 *
 * D3D11, located in various files.
 *
 */

#ifdef XR_USE_GRAPHICS_API_D3D11

XrResult
oxr_d3d11_get_requirements(struct oxr_logger *log,
                           struct oxr_system *sys,
                           XrGraphicsRequirementsD3D11KHR *graphicsRequirements);

/**
 * @brief Check to ensure the device provided at session create matches the LUID we returned earlier.
 *
 * @return XR_SUCCESS if the device matches the LUID
 */
XrResult
oxr_d3d11_check_device(struct oxr_logger *log, struct oxr_system *sys, ID3D11Device *device);


XrResult
oxr_session_populate_d3d11(struct oxr_logger *log,
                           struct oxr_system *sys,
                           XrGraphicsBindingD3D11KHR const *next,
                           struct oxr_session *sess);

XrResult
oxr_swapchain_d3d11_create(struct oxr_logger *,
                           struct oxr_session *sess,
                           const XrSwapchainCreateInfo *,
                           struct oxr_swapchain **out_swapchain);

#endif

/*
 *
 * D3D12, located in various files.
 *
 */

#ifdef XR_USE_GRAPHICS_API_D3D12

XrResult
oxr_d3d12_get_requirements(struct oxr_logger *log,
                           struct oxr_system *sys,
                           XrGraphicsRequirementsD3D12KHR *graphicsRequirements);

/**
 * @brief Check to ensure the device provided at session create matches the LUID we returned earlier.
 *
 * @return XR_SUCCESS if the device matches the LUID
 */
XrResult
oxr_d3d12_check_device(struct oxr_logger *log, struct oxr_system *sys, ID3D12Device *device);


XrResult
oxr_session_populate_d3d12(struct oxr_logger *log,
                           struct oxr_system *sys,
                           XrGraphicsBindingD3D12KHR const *next,
                           struct oxr_session *sess);

XrResult
oxr_swapchain_d3d12_create(struct oxr_logger *,
                           struct oxr_session *sess,
                           const XrSwapchainCreateInfo *,
                           struct oxr_swapchain **out_swapchain);

#endif

/*
 *
 * Structs
 *
 */


/*!
 * Used to hold diverse child handles and ensure orderly destruction.
 *
 * Each object referenced by an OpenXR handle should have one of these as its
 * first element, thus "extending" this class.
 */
struct oxr_handle_base
{
	//! Magic (per-handle-type) value for debugging.
	uint64_t debug;

	/*!
	 * Pointer to this object's parent handle holder, if any.
	 */
	struct oxr_handle_base *parent;

	/*!
	 * Array of children, if any.
	 */
	struct oxr_handle_base *children[XRT_MAX_HANDLE_CHILDREN];

	/*!
	 * Current handle state.
	 */
	enum oxr_handle_state state;

	/*!
	 * Destroy the object this handle refers to.
	 */
	oxr_handle_destroyer destroy;
};

/*!
 * Single or multiple devices grouped together to form a system that sessions
 * can be created from. Might need to open devices to get all
 * properties from it, but shouldn't.
 *
 * Not strictly an object, but an atom.
 *
 * Valid only within a XrInstance (@ref oxr_instance)
 *
 * @obj{XrSystemId}
 */
struct oxr_system
{
	struct oxr_instance *inst;

	//! The @ref xrt_iface level system.
	struct xrt_system *xsys;

	//! System devices used in all session types.
	struct xrt_system_devices *xsysd;

	//! Space overseer used in all session types.
	struct xrt_space_overseer *xso;

	//! System compositor, used to create session compositors.
	struct xrt_system_compositor *xsysc;

	XrSystemId systemId;

	//! Have the client application called the gfx api requirements func?
	bool gotten_requirements;

	XrFormFactor form_factor;
	XrViewConfigurationType view_config_type;
	XrViewConfigurationView views[2];
	uint32_t blend_mode_count;
	XrEnvironmentBlendMode blend_modes[3];

	XrReferenceSpaceType reference_spaces[5];
	uint32_t reference_space_count;

	//! Cache of the last known system roles, see @ref xrt_system_roles::generation_id
	struct xrt_system_roles dynamic_roles_cache;
	struct os_mutex sync_actions_mutex;

	struct xrt_visibility_mask *visibility_mask[2];

#ifdef OXR_HAVE_MNDX_xdev_space
	bool supports_xdev_space;
#endif

#ifdef XR_USE_GRAPHICS_API_VULKAN
	//! The instance/device we create when vulkan_enable2 is used
	VkInstance vulkan_enable2_instance;
	//! The device returned with the last xrGetVulkanGraphicsDeviceKHR or xrGetVulkanGraphicsDevice2KHR call.
	//! XR_NULL_HANDLE if neither has been called.
	VkPhysicalDevice suggested_vulkan_physical_device;

	struct
	{
		// No better place to keep this state.
		bool external_fence_fd_enabled;
		bool external_semaphore_fd_enabled;
		bool timeline_semaphore_enabled;
		bool debug_utils_enabled;
	} vk;

#endif

#if defined(XR_USE_GRAPHICS_API_D3D11) || defined(XR_USE_GRAPHICS_API_D3D12)
	LUID suggested_d3d_luid;
	bool suggested_d3d_luid_valid;
#endif
};


/*
 * Device roles helpers.
 */

// static roles
// clang-format off
static inline struct xrt_device *get_role_head(struct oxr_system *sys) {return sys->xsysd->static_roles.head; }
static inline struct xrt_device *get_role_eyes(struct oxr_system *sys) {return sys->xsysd->static_roles.eyes; }
static inline struct xrt_device *get_role_face(struct oxr_system* sys) { return sys->xsysd->static_roles.face; }
static inline struct xrt_device *get_role_body(struct oxr_system* sys) { return sys->xsysd->static_roles.body; }
static inline struct xrt_device *get_role_hand_tracking_left(struct oxr_system* sys) { return sys->xsysd->static_roles.hand_tracking.left; }
static inline struct xrt_device *get_role_hand_tracking_right(struct oxr_system* sys) { return sys->xsysd->static_roles.hand_tracking.right; }
// clang-format on

// dynamic roles
#define MAKE_GET_DYN_ROLES_FN(ROLE)                                                                                    \
	static inline struct xrt_device *get_role_##ROLE(struct oxr_system *sys)                                       \
	{                                                                                                              \
		const bool is_locked = 0 == os_mutex_trylock(&sys->sync_actions_mutex);                                \
		const int32_t xdev_idx = sys->dynamic_roles_cache.ROLE;                                                \
		if (is_locked) {                                                                                       \
			os_mutex_unlock(&sys->sync_actions_mutex);                                                     \
		}                                                                                                      \
		if (xdev_idx < 0 || xdev_idx >= (int32_t)ARRAY_SIZE(sys->xsysd->xdevs))                                \
			return NULL;                                                                                   \
		return sys->xsysd->xdevs[xdev_idx];                                                                    \
	}
MAKE_GET_DYN_ROLES_FN(left)
MAKE_GET_DYN_ROLES_FN(right)
MAKE_GET_DYN_ROLES_FN(gamepad)
#undef MAKE_GET_DYN_ROLES_FN

#define GET_XDEV_BY_ROLE(SYS, ROLE) (get_role_##ROLE((SYS)))


static inline enum xrt_device_name
get_role_profile_head(struct oxr_system *sys)
{
	return XRT_DEVICE_INVALID;
}
static inline enum xrt_device_name
get_role_profile_eyes(struct oxr_system *sys)
{
	return XRT_DEVICE_INVALID;
}
static inline enum xrt_device_name
get_role_profile_face(struct oxr_system *sys)
{
	return XRT_DEVICE_INVALID;
}
static inline enum xrt_device_name
get_role_profile_body(struct oxr_system *sys)
{
	return XRT_DEVICE_INVALID;
}
static inline enum xrt_device_name
get_role_profile_hand_tracking_left(struct oxr_system *sys)
{
	return XRT_DEVICE_INVALID;
}
static inline enum xrt_device_name
get_role_profile_hand_tracking_right(struct oxr_system *sys)
{
	return XRT_DEVICE_INVALID;
}

#define MAKE_GET_DYN_ROLE_PROFILE_FN(ROLE)                                                                             \
	static inline enum xrt_device_name get_role_profile_##ROLE(struct oxr_system *sys)                             \
	{                                                                                                              \
		const bool is_locked = 0 == os_mutex_trylock(&sys->sync_actions_mutex);                                \
		const enum xrt_device_name profile_name = sys->dynamic_roles_cache.ROLE##_profile;                     \
		if (is_locked) {                                                                                       \
			os_mutex_unlock(&sys->sync_actions_mutex);                                                     \
		}                                                                                                      \
		return profile_name;                                                                                   \
	}
MAKE_GET_DYN_ROLE_PROFILE_FN(left)
MAKE_GET_DYN_ROLE_PROFILE_FN(right)
MAKE_GET_DYN_ROLE_PROFILE_FN(gamepad)
#undef MAKE_GET_DYN_ROLES_FN

#define GET_PROFILE_NAME_BY_ROLE(SYS, ROLE) (get_role_profile_##ROLE((SYS)))

/*
 * Extensions helpers.
 */

#define MAKE_EXT_STATUS(mixed_case, all_caps) bool mixed_case;
/*!
 * Structure tracking which extensions are enabled for a given instance.
 *
 * Names are systematic: the extension name with the XR_ prefix removed.
 */
struct oxr_extension_status
{
	OXR_EXTENSION_SUPPORT_GENERATE(MAKE_EXT_STATUS)
};
#undef MAKE_EXT_STATUS

/*!
 * Main object that ties everything together.
 *
 * No parent type/handle: this is the root handle.
 *
 * @obj{XrInstance}
 * @extends oxr_handle_base
 */
struct oxr_instance
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	struct u_debug_gui *debug_ui;

	struct xrt_instance *xinst;

	//! Enabled extensions
	struct oxr_extension_status extensions;

	// Hardcoded single system.
	struct oxr_system system;

	struct time_state *timekeeping;

	struct
	{
		struct u_hashset *name_store;
		struct u_hashset *loc_store;
	} action_sets;

	//! Path store, for looking up paths.
	struct u_hashset *path_store;
	//! Mapping from ID to path.
	struct oxr_path **path_array;
	//! Total length of path array.
	size_t path_array_length;
	//! Number of paths in the array (0 is always null).
	size_t path_num;

	// Event queue.
	struct
	{
		struct os_mutex mutex;
		struct oxr_event *last;
		struct oxr_event *next;
	} event;

	//! Interaction profile bindings that have been suggested by the client.
	struct oxr_interaction_profile **profiles;
	size_t profile_count;

	struct oxr_session *sessions;

	struct
	{

#define SUBACTION_PATH_MEMBER(X) XrPath X;
		OXR_FOR_EACH_SUBACTION_PATH(SUBACTION_PATH_MEMBER)

#undef SUBACTION_PATH_MEMBER


		XrPath khr_simple_controller;
		XrPath google_daydream_controller;
		XrPath htc_vive_controller;
		XrPath htc_vive_pro;
		XrPath microsoft_motion_controller;
		XrPath microsoft_xbox_controller;
		XrPath oculus_go_controller;
		XrPath oculus_touch_controller;
		XrPath valve_index_controller;
		XrPath hp_mixed_reality_controller;
		XrPath samsung_odyssey_controller;
		XrPath ml_ml2_controller;
		XrPath mndx_ball_on_a_stick_controller;
		XrPath msft_hand_interaction;
		XrPath ext_eye_gaze_interaction;
		XrPath ext_hand_interaction;
		XrPath oppo_mr_controller;
	} path_cache;

	struct
	{
		struct
		{
			struct
			{
				uint32_t major;
				uint32_t minor;
				uint32_t patch;
				const char *name; //< Engine name, not freed.
			} engine;
		} detected;
	} appinfo;

	struct
	{
		//! Unreal has a bug in the VulkanRHI backend.
		bool disable_vulkan_format_depth_stencil;
		//! Unreal 4 has a bug calling xrEndSession; the function should just exit
		bool skip_end_session;

		/*!
		 * Return XR_ERROR_REFERENCE_SPACE_UNSUPPORTED instead of
		 * XR_ERROR_VALIDATION_FAILURE in xrCreateReferenceSpace.
		 */
		bool no_validation_error_in_create_ref_space;
	} quirks;

	//! Debug messengers
	struct oxr_debug_messenger *messengers[XRT_MAX_HANDLE_CHILDREN];

	bool lifecycle_verbose;
	bool debug_views;
	bool debug_spaces;
	bool debug_bindings;

#ifdef XRT_FEATURE_RENDERDOC
	RENDERDOC_API_1_4_1 *rdoc_api;
#endif
};

/*!
 * Object that client program interact with.
 *
 * Parent type/handle is @ref oxr_instance
 *
 * @obj{XrSession}
 * @extends oxr_handle_base
 */
struct oxr_session
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;
	struct oxr_system *sys;

	//! What graphics type was this session created with.
	enum oxr_session_graphics_ext gfx_ext;

	//! The @ref xrt_session backing this session.
	struct xrt_session *xs;

	//! Native compositor that is wrapped by client compositors.
	struct xrt_compositor_native *xcn;

	struct xrt_compositor *compositor;

	struct oxr_session *next;

	XrSessionState state;
	bool has_begun;
	/*!
	 * There is a extra state between xrBeginSession has been called and
	 * the first xrEndFrame has been called. These are to track this.
	 */
	bool has_ended_once;

	bool compositor_visible;
	bool compositor_focused;

	// the number of xrWaitFrame calls that did not yet have a corresponding
	// xrEndFrame or xrBeginFrame (discarded frame) call
	int active_wait_frames;
	struct os_mutex active_wait_frames_lock;

	bool frame_started;
	bool exiting;

	struct
	{
		int64_t waited;
		int64_t begun;
	} frame_id;

	struct os_semaphore sem;

	/*!
	 * Used to implement precise extra sleeping in wait frame.
	 */
	struct os_precise_sleeper sleeper;

	/*!
	 * An array of action set attachments that this session owns.
	 *
	 * If non-null, this means action sets have been attached to this
	 * session.
	 */
	struct oxr_action_set_attachment *act_set_attachments;

	/*!
	 * Length of @ref oxr_session::act_set_attachments.
	 */
	size_t action_set_attachment_count;

	/*!
	 * A map of action set key to action set attachments.
	 *
	 * If non-null, this means action sets have been attached to this
	 * session, since this map points to elements of
	 * oxr_session::act_set_attachments
	 */
	struct u_hashmap_int *act_sets_attachments_by_key;

	/*!
	 * A map of action key to action attachment.
	 *
	 * The action attachments are actually owned by the action set
	 * attachments, but we own the action set attachments, so this is OK.
	 *
	 * If non-null, this means action sets have been attached to this
	 * session, since this map points to @p oxr_action_attachment members of
	 * @ref oxr_session::act_set_attachments elements.
	 */
	struct u_hashmap_int *act_attachments_by_key;

	/*!
	 * Clone of all suggested binding profiles at the point of action set/session attachment.
	 * @ref oxr_session_attach_action_sets
	 */
	size_t profiles_on_attachment_size;
	struct oxr_interaction_profile **profiles_on_attachment;

	/*!
	 * Currently bound interaction profile.
	 * @{
	 */

#define OXR_PATH_MEMBER(X) XrPath X;

	OXR_FOR_EACH_VALID_SUBACTION_PATH(OXR_PATH_MEMBER)
#undef OXR_PATH_MEMBER
	/*!
	 * @}
	 */

	/*!
	 * IPD, to be expanded to a proper 3D relation.
	 */
	float ipd_meters;

	/*!
	 * Frame timing debug output.
	 */
	bool frame_timing_spew;

	//! Extra sleep in wait frame.
	uint32_t frame_timing_wait_sleep_ms;

	/*!
	 * To pipe swapchain creation to right code.
	 */
	XrResult (*create_swapchain)(struct oxr_logger *,
	                             struct oxr_session *sess,
	                             const XrSwapchainCreateInfo *,
	                             struct oxr_swapchain **);

	/*! initial relation of head in "global" space.
	 * Used as reference for local space.  */
	struct xrt_space_relation local_space_pure_relation;

	bool has_lost;
};

/*!
 * Returns XR_SUCCESS or XR_SESSION_LOSS_PENDING as appropriate.
 *
 * @public @memberof oxr_session
 */
static inline XrResult
oxr_session_success_result(struct oxr_session *session)
{
	switch (session->state) {
	case XR_SESSION_STATE_LOSS_PENDING: return XR_SESSION_LOSS_PENDING;
	default: return XR_SUCCESS;
	}
}

/*!
 * Returns XR_SUCCESS, XR_SESSION_LOSS_PENDING, or XR_SESSION_NOT_FOCUSED, as
 * appropriate.
 *
 * @public @memberof oxr_session
 */
static inline XrResult
oxr_session_success_focused_result(struct oxr_session *session)
{
	switch (session->state) {
	case XR_SESSION_STATE_LOSS_PENDING: return XR_SESSION_LOSS_PENDING;
	case XR_SESSION_STATE_FOCUSED: return XR_SUCCESS;
	default: return XR_SESSION_NOT_FOCUSED;
	}
}

#ifdef OXR_HAVE_FB_display_refresh_rate
XrResult
oxr_session_get_display_refresh_rate(struct oxr_logger *log, struct oxr_session *sess, float *displayRefreshRate);

XrResult
oxr_session_request_display_refresh_rate(struct oxr_logger *log, struct oxr_session *sess, float displayRefreshRate);
#endif // OXR_HAVE_FB_display_refresh_rate

/*!
 * dpad settings we need extracted from XrInteractionProfileDpadBindingEXT
 *
 * @ingroup oxr_input
 */
struct oxr_dpad_settings
{
	float forceThreshold;
	float forceThresholdReleased;
	float centerRegion;
	float wedgeAngle;
	bool isSticky;
};

/*!
 * dpad binding extracted from XrInteractionProfileDpadBindingEXT
 */
struct oxr_dpad_binding_modification
{
	XrPath binding;
	struct oxr_dpad_settings settings;
};

/*!
 * A entry in the dpad state for one action set.
 *
 * @ingroup oxr_input
 */
struct oxr_dpad_entry
{
#ifdef XR_EXT_dpad_binding
	struct oxr_dpad_binding_modification dpads[4];
	uint32_t dpad_count;
#endif

	uint64_t key;
};

/*!
 * Holds dpad binding state for a single interaction profile.
 *
 * @ingroup oxr_input
 */
struct oxr_dpad_state
{
	struct u_hashmap_int *uhi;
};

/*!
 * dpad emulation settings from oxr_interaction_profile
 */
struct oxr_dpad_emulation
{
	enum oxr_subaction_path subaction_path;
	XrPath *paths;
	uint32_t path_count;
	enum xrt_input_name position;
	enum xrt_input_name activate; // Can be zero
};

/*!
 * A single interaction profile.
 */
struct oxr_interaction_profile
{
	XrPath path;

	//! Used to lookup @ref xrt_binding_profile for fallback.
	enum xrt_device_name xname;

	//! Name presented to the user.
	const char *localized_name;

	struct oxr_binding *bindings;
	size_t binding_count;

	struct oxr_dpad_emulation *dpads;
	size_t dpad_count;

	struct oxr_dpad_state dpad_state;
};

/*!
 * Interaction profile binding state.
 */
struct oxr_binding
{
	XrPath *paths;
	uint32_t path_count;

	//! Name presented to the user.
	const char *localized_name;

	enum oxr_subaction_path subaction_path;

	uint32_t key_count;
	uint32_t *keys;
	//! store which entry in paths was suggested, for each action key
	uint32_t *preferred_binding_path_index;

	enum xrt_input_name input;
	enum xrt_input_name dpad_activate;

	enum xrt_output_name output;
};

/*!
 * @defgroup oxr_input OpenXR input
 * @ingroup oxr_main
 *
 * @brief The action-set/action-based input subsystem of OpenXR.
 *
 *
 * Action sets are created as children of the Instance, but are primarily used
 * with one or more Sessions. They may be used with multiple sessions at a time,
 * so we can't just put the per-session information directly in the action set
 * or action. Instead, we have the `_attachment `structures, which mirror the
 * action sets and actions but are rooted under the Session:
 *
 * - For every action set attached to a session, that session owns a @ref
 *   oxr_action_set_attachment.
 * - For each action in those attached action sets, the action set attachment
 *   owns an @ref oxr_action_attachment.
 *
 * We go from the public handle to the `_attachment` structure by using a `key`
 * value and a hash map: specifically, we look up the
 * oxr_action_set::act_set_key and oxr_action::act_key in the session.
 *
 * ![](monado-input-class-relationships.drawio.svg)
 */

/*!
 * A parsed equivalent of a list of sub-action paths.
 *
 * If @p any is true, then no paths were provided, which typically means any
 * input is acceptable.
 *
 * @ingroup oxr_main
 * @ingroup oxr_input
 */
struct oxr_subaction_paths
{
	bool any;
#define OXR_SUBPATH_MEMBER(X) bool X;
	OXR_FOR_EACH_SUBACTION_PATH(OXR_SUBPATH_MEMBER)
#undef OXR_SUBPATH_MEMBER
};

/*!
 * Helper function to determine if the set of paths in @p a is a subset of the
 * paths in @p b.
 *
 * @public @memberof oxr_subaction_paths
 */
static inline bool
oxr_subaction_paths_is_subset_of(const struct oxr_subaction_paths *a, const struct oxr_subaction_paths *b)
{
#define OXR_CHECK_SUBACTION_PATHS(X)                                                                                   \
	if (a->X && !b->X) {                                                                                           \
		return false;                                                                                          \
	}
	OXR_FOR_EACH_SUBACTION_PATH(OXR_CHECK_SUBACTION_PATHS)
#undef OXR_CHECK_SUBACTION_PATHS
	return true;
}

/*!
 * The data associated with the attachment of an Action Set (@ref
 * oxr_action_set) to as Session (@ref oxr_session).
 *
 * This structure has no pointer to the @ref oxr_action_set that created it
 * because the application is allowed to destroy an action before the session,
 * which should change nothing except not allow the application to use the
 * corresponding data anymore.
 *
 * @ingroup oxr_input
 *
 * @see oxr_action_set
 */
struct oxr_action_set_attachment
{
	//! Owning session.
	struct oxr_session *sess;

	//! Action set refcounted data
	struct oxr_action_set_ref *act_set_ref;

	//! Unique key for the session hashmap.
	uint32_t act_set_key;

	//! Which sub-action paths are requested on the latest sync.
	struct oxr_subaction_paths requested_subaction_paths;

	//! An array of action attachments we own.
	struct oxr_action_attachment *act_attachments;

	/*!
	 * Length of @ref oxr_action_set_attachment::act_attachments.
	 */
	size_t action_attachment_count;
};

/*!
 * De-initialize an action set attachment and its action attachments.
 *
 * Frees the action attachments, but does not de-allocate the action set
 * attachment.
 *
 * @public @memberof oxr_action_set_attachment
 */
void
oxr_action_set_attachment_teardown(struct oxr_action_set_attachment *act_set_attached);


/*!
 * The state of a action input.
 *
 * @ingroup oxr_input
 *
 * @see oxr_action_attachment
 */
struct oxr_action_state
{
	/*!
	 * The actual value - must interpret using action type
	 */
	union xrt_input_value value;

	//! Is this active (bound and providing input)?
	bool active;

	// Was this changed.
	bool changed;

	//! When was this last changed.
	XrTime timestamp;
};

/*!
 * A input action pair of a @ref xrt_input and a @ref xrt_device, along with the
 * required transform.
 *
 * @ingroup oxr_input
 *
 * @see xrt_device
 * @see xrt_input
 */
struct oxr_action_input
{
	struct xrt_device *xdev;                // Used for poses and transform is null.
	struct xrt_input *input;                // Ditto
	enum xrt_input_name dpad_activate_name; // used to activate dpad emulation if present
	struct xrt_input *dpad_activate;        // used to activate dpad emulation if present
	struct oxr_input_transform *transforms;
	size_t transform_count;
	XrPath bound_path;
};

/*!
 * A output action pair of a @ref xrt_output_name and a @ref xrt_device.
 *
 * @ingroup oxr_input
 *
 * @see xrt_device
 * @see xrt_output_name
 */
struct oxr_action_output
{
	struct xrt_device *xdev;
	enum xrt_output_name name;
	XrPath bound_path;
};


/*!
 * The set of inputs/outputs for a single sub-action path for an action.
 *
 * Each @ref oxr_action_attachment has one of these for every known sub-action
 * path in the spec. Many, or even most, will be "empty".
 *
 * A single action will either be input or output, not both.
 *
 * @ingroup oxr_input
 *
 * @see oxr_action_attachment
 */
struct oxr_action_cache
{
	struct oxr_action_state current;

	size_t input_count;
	struct oxr_action_input *inputs;

	int64_t stop_output_time;
	size_t output_count;
	struct oxr_action_output *outputs;
};

/*!
 * Data associated with an Action that has been attached to a Session.
 *
 * More information on the action vs action attachment and action set vs action
 * set attachment parallel is in the docs for @ref oxr_input
 *
 * @ingroup oxr_input
 *
 * @see oxr_action
 */
struct oxr_action_attachment
{
	//! The owning action set attachment
	struct oxr_action_set_attachment *act_set_attached;

	//! This action's refcounted data
	struct oxr_action_ref *act_ref;

	/*!
	 * The corresponding session.
	 *
	 * This will always be valid: the session outlives this object because
	 * it owns act_set_attached.
	 */
	struct oxr_session *sess;

	//! Unique key for the session hashmap.
	uint32_t act_key;


	/*!
	 * For pose actions any subaction paths are special treated, at bind
	 * time we pick one subaction path and stick to it as long as the action
	 * lives.
	 */
	struct oxr_subaction_paths any_pose_subaction_path;

	struct oxr_action_state any_state;

#define OXR_CACHE_MEMBER(X) struct oxr_action_cache X;
	OXR_FOR_EACH_SUBACTION_PATH(OXR_CACHE_MEMBER)
#undef OXR_CACHE_MEMBER
};

/*!
 * @}
 */


static inline bool
oxr_space_type_is_reference(enum oxr_space_type space_type)
{
	switch (space_type) {
	case OXR_SPACE_TYPE_REFERENCE_VIEW:
	case OXR_SPACE_TYPE_REFERENCE_LOCAL:
	case OXR_SPACE_TYPE_REFERENCE_LOCAL_FLOOR:
	case OXR_SPACE_TYPE_REFERENCE_STAGE:
	case OXR_SPACE_TYPE_REFERENCE_UNBOUNDED_MSFT:
	case OXR_SPACE_TYPE_REFERENCE_COMBINED_EYE_VARJO:
	case OXR_SPACE_TYPE_REFERENCE_LOCALIZATION_MAP_ML:
		// These are reference spaces.
		return true;

	case OXR_SPACE_TYPE_ACTION:
	case OXR_SPACE_TYPE_XDEV_POSE:
		// These are not reference spaces.
		return false;
	}

	// Handles invalid value.
	return false;
}


/*!
 * Can be one of several reference space types, or a space that is bound to an
 * action.
 *
 * Parent type/handle is @ref oxr_session
 *
 * @obj{XrSpace}
 * @extends oxr_handle_base
 */
struct oxr_space
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this space.
	struct oxr_session *sess;

	//! Pose that was given during creation.
	struct xrt_pose pose;

	//! Action key from which action this space was created from.
	uint32_t act_key;

	//! What kind of space is this?
	enum oxr_space_type space_type;

	//! Which sub action path is this?
	struct oxr_subaction_paths subaction_paths;

	struct
	{
		struct xrt_space *xs;
		struct xrt_device *xdev;
		enum xrt_input_name name;
	} action;

	struct
	{
		struct xrt_space *xs;
	} xdev_pose;
};

/*!
 * A set of images used for rendering.
 *
 * Parent type/handle is @ref oxr_session
 *
 * @obj{XrSwapchain}
 * @extends oxr_handle_base
 */
struct oxr_swapchain
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this swapchain.
	struct oxr_session *sess;

	//! Compositor swapchain.
	struct xrt_swapchain *swapchain;

	//! Swapchain size.
	uint32_t width, height;

	//! For 1 is 2D texture, greater then 1 2D array texture.
	uint32_t array_layer_count;

	//! The number of cubemap faces.  6 for cubemaps, 1 otherwise.
	uint32_t face_count;

	struct
	{
		enum oxr_image_state state;
	} images[XRT_MAX_SWAPCHAIN_IMAGES];

	struct
	{
		size_t num;
		struct u_index_fifo fifo;
	} acquired;

	struct
	{
		bool yes;
		int index;
	} inflight; // This is the image that the app is working on.

	struct
	{
		bool yes;
		int index;
	} released;

	// Is this a static swapchain, needed for acquire semantics.
	bool is_static;


	XrResult (*destroy)(struct oxr_logger *, struct oxr_swapchain *);

	XrResult (*enumerate_images)(struct oxr_logger *,
	                             struct oxr_swapchain *,
	                             uint32_t,
	                             XrSwapchainImageBaseHeader *);

	XrResult (*acquire_image)(struct oxr_logger *,
	                          struct oxr_swapchain *,
	                          const XrSwapchainImageAcquireInfo *,
	                          uint32_t *);

	XrResult (*wait_image)(struct oxr_logger *, struct oxr_swapchain *, const XrSwapchainImageWaitInfo *);

	XrResult (*release_image)(struct oxr_logger *, struct oxr_swapchain *, const XrSwapchainImageReleaseInfo *);
};

struct oxr_refcounted
{
	struct xrt_reference base;
	//! Destruction callback
	void (*destroy)(struct oxr_refcounted *);
};

/*!
 * Increase the reference count of @p orc.
 */
static inline void
oxr_refcounted_ref(struct oxr_refcounted *orc)
{
	xrt_reference_inc(&orc->base);
}

/*!
 * Decrease the reference count of @p orc, destroying it if it reaches 0.
 */
static inline void
oxr_refcounted_unref(struct oxr_refcounted *orc)
{
	if (xrt_reference_dec_and_is_zero(&orc->base)) {
		orc->destroy(orc);
	}
}

/*!
 * The reference-counted data of an action set.
 *
 * One or more sessions may still need this data after the application destroys
 * its XrActionSet handle, so this data is refcounted.
 *
 * @ingroup oxr_input
 *
 * @see oxr_action_set
 * @extends oxr_refcounted
 */
struct oxr_action_set_ref
{
	struct oxr_refcounted base;

	//! Application supplied name of this action.
	char name[XR_MAX_ACTION_SET_NAME_SIZE];

	/*!
	 * Has this action set even been attached to any session, marking it as
	 * immutable.
	 */
	bool ever_attached;

	//! Unique key for the session hashmap.
	uint32_t act_set_key;

	//! Application supplied action set priority.
	uint32_t priority;

	struct
	{
		struct u_hashset *name_store;
		struct u_hashset *loc_store;
	} actions;

	struct oxr_subaction_paths permitted_subaction_paths;
};

/*!
 * A group of actions.
 *
 * Parent type/handle is @ref oxr_instance
 *
 * Note, however, that an action set must be "attached" to a session
 * ( @ref oxr_session ) to be used and not just configured.
 * The corresponding data is in @ref oxr_action_set_attachment.
 *
 * @ingroup oxr_input
 *
 * @obj{XrActionSet}
 * @extends oxr_handle_base
 */
struct oxr_action_set
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this action set.
	struct oxr_instance *inst;

	/*!
	 * The data for this action set that must live as long as any session we
	 * are attached to.
	 */
	struct oxr_action_set_ref *data;


	/*!
	 * Unique key for the session hashmap.
	 *
	 * Duplicated from oxr_action_set_ref::act_set_key for efficiency.
	 */
	uint32_t act_set_key;

	//! The item in the name hashset.
	struct u_hashset_item *name_item;

	//! The item in the localized hashset.
	struct u_hashset_item *loc_item;
};

/*!
 * The reference-counted data of an action.
 *
 * One or more sessions may still need this data after the application destroys
 * its XrAction handle, so this data is refcounted.
 *
 * @ingroup oxr_input
 *
 * @see oxr_action
 * @extends oxr_refcounted
 */
struct oxr_action_ref
{
	struct oxr_refcounted base;

	//! Application supplied name of this action.
	char name[XR_MAX_ACTION_NAME_SIZE];

	//! Unique key for the session hashmap.
	uint32_t act_key;

	//! Type this action was created with.
	XrActionType action_type;

	//! Which sub action paths that this action was created with.
	struct oxr_subaction_paths subaction_paths;
};

/*!
 * A single action.
 *
 * Parent type/handle is @ref oxr_action_set
 *
 * For actual usage, an action is attached to a session: the corresponding data
 * is in @ref oxr_action_attachment
 *
 * @ingroup oxr_input
 *
 * @obj{XrAction}
 * @extends oxr_handle_base
 */
struct oxr_action
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this action.
	struct oxr_action_set *act_set;

	//! The data for this action that must live as long as any session we
	//! are attached to.
	struct oxr_action_ref *data;

	/*!
	 * Unique key for the session hashmap.
	 *
	 * Duplicated from oxr_action_ref::act_key for efficiency.
	 */
	uint32_t act_key;

	//! The item in the name hashset.
	struct u_hashset_item *name_item;

	//! The item in the localized hashset.
	struct u_hashset_item *loc_item;
};

/*!
 * Debug object created by the client program.
 *
 * Parent type/handle is @ref oxr_instance
 *
 * @obj{XrDebugUtilsMessengerEXT}
 */
struct oxr_debug_messenger
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this messenger.
	struct oxr_instance *inst;

	//! Severities to submit to this messenger
	XrDebugUtilsMessageSeverityFlagsEXT message_severities;

	//! Types to submit to this messenger
	XrDebugUtilsMessageTypeFlagsEXT message_types;

	//! Callback function
	PFN_xrDebugUtilsMessengerCallbackEXT user_callback;

	//! Opaque user data
	void *XR_MAY_ALIAS user_data;
};

/*!
 * A hand tracker.
 *
 * Parent type/handle is @ref oxr_instance
 *
 *
 * @obj{XrHandTrackerEXT}
 * @extends oxr_handle_base
 */
struct oxr_hand_tracker
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this hand tracker.
	struct oxr_session *sess;

	//! xrt_device backing this hand tracker
	struct xrt_device *xdev;

	//! the input name associated with this hand tracker
	enum xrt_input_name input_name;

	XrHandEXT hand;
	XrHandJointSetEXT hand_joint_set;
};

#ifdef OXR_HAVE_FB_passthrough

struct oxr_passthrough
{
	struct oxr_handle_base handle;

	struct oxr_session *sess;

	XrPassthroughFlagsFB flags;

	bool paused;
};

struct oxr_passthrough_layer
{
	struct oxr_handle_base handle;

	struct oxr_session *sess;

	XrPassthroughFB passthrough;

	XrPassthroughFlagsFB flags;

	XrPassthroughLayerPurposeFB purpose;

	bool paused;

	XrPassthroughStyleFB style;
	XrPassthroughColorMapMonoToRgbaFB monoToRgba;
	XrPassthroughColorMapMonoToMonoFB monoToMono;
	XrPassthroughBrightnessContrastSaturationFB brightnessContrastSaturation;
};

XrResult
oxr_passthrough_create(struct oxr_logger *log,
                       struct oxr_session *sess,
                       const XrPassthroughCreateInfoFB *createInfo,
                       struct oxr_passthrough **out_passthrough);

XrResult
oxr_passthrough_layer_create(struct oxr_logger *log,
                             struct oxr_session *sess,
                             const XrPassthroughLayerCreateInfoFB *createInfo,
                             struct oxr_passthrough_layer **out_layer);

static inline XrPassthroughFB
oxr_passthrough_to_openxr(struct oxr_passthrough *passthrough)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrPassthroughFB, passthrough);
}

static inline XrPassthroughLayerFB
oxr_passthrough_layer_to_openxr(struct oxr_passthrough_layer *passthroughLayer)
{
	return XRT_CAST_PTR_TO_OXR_HANDLE(XrPassthroughLayerFB, passthroughLayer);
}

XrResult
oxr_event_push_XrEventDataPassthroughStateChangedFB(struct oxr_logger *log,
                                                    struct oxr_session *sess,
                                                    XrPassthroughStateChangedFlagsFB flags);

#endif // OXR_HAVE_FB_passthrough

/*!
 * HTC specific Facial tracker.
 *
 * Parent type/handle is @ref oxr_instance
 *
 *
 * @obj{XrFacialTrackerHTC}
 * @extends oxr_handle_base
 */
struct oxr_facial_tracker_htc
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this face tracker.
	struct oxr_session *sess;

	//! xrt_device backing this face tracker
	struct xrt_device *xdev;

	//! Type of facial tracking, eyes or lips
	enum xrt_facial_tracking_type_htc facial_tracking_type;
};

#ifdef OXR_HAVE_FB_body_tracking
/*!
 * FB specific Body tracker.
 *
 * Parent type/handle is @ref oxr_instance
 *
 * @obj{XrBodyTrackerFB}
 * @extends oxr_handle_base
 */
struct oxr_body_tracker_fb
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this face tracker.
	struct oxr_session *sess;

	//! xrt_device backing this face tracker
	struct xrt_device *xdev;

	//! Type of the body joint set e.g. XR_FB_body_tracking or XR_META_body_tracking_full_body
	enum xrt_body_joint_set_type_fb joint_set_type;
};

XrResult
oxr_create_body_tracker_fb(struct oxr_logger *log,
                           struct oxr_session *sess,
                           const XrBodyTrackerCreateInfoFB *createInfo,
                           struct oxr_body_tracker_fb **out_body_tracker_fb);

XrResult
oxr_get_body_skeleton_fb(struct oxr_logger *log,
                         struct oxr_body_tracker_fb *body_tracker_fb,
                         XrBodySkeletonFB *skeleton);

XrResult
oxr_locate_body_joints_fb(struct oxr_logger *log,
                          struct oxr_body_tracker_fb *body_tracker_fb,
                          struct oxr_space *base_spc,
                          const XrBodyJointsLocateInfoFB *locateInfo,
                          XrBodyJointLocationsFB *locations);
#endif

#ifdef OXR_HAVE_MNDX_xdev_space
/*!
 * Object that holds a list of the current @ref xrt_devices.
 *
 * Parent type/handle is @ref oxr_session
 *
 * @obj{XrXDevList}
 * @extends oxr_handle_base
 */
struct oxr_xdev_list
{
	//! Common structure for things referred to by OpenXR handles.
	struct oxr_handle_base handle;

	//! Owner of this @ref xrt_device list.
	struct oxr_session *sess;

	//! Monotonically increasing number.
	uint64_t generation_number;

	uint64_t ids[XRT_SYSTEM_MAX_DEVICES];
	struct xrt_device *xdevs[XRT_SYSTEM_MAX_DEVICES];
	enum xrt_input_name names[XRT_SYSTEM_MAX_DEVICES];

	//! Counts ids, names and xdevs.
	uint32_t device_count;
};
#endif // OXR_HAVE_MNDX_xdev_space

/*!
 * @}
 */


#ifdef __cplusplus
}
#endif

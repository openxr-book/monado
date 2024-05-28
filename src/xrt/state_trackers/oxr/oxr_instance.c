// Copyright 2018-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Holds instance related functions.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup oxr_main
 */

#include "xrt/xrt_config_os.h"
#include "xrt/xrt_config_build.h"
#include "xrt/xrt_instance.h"

#include "math/m_mathinclude.h"
#include "util/u_var.h"
#include "util/u_time.h"
#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_git_tag.h"
#include "util/u_builders.h"

#ifdef XRT_FEATURE_CLIENT_DEBUG_GUI
#include "util/u_debug_gui.h"
#endif

#ifdef XRT_OS_ANDROID
#include "android/android_globals.h"
#include "android/android_looper.h"
#endif

#include "oxr_objects.h"
#include "oxr_logger.h"
#include "oxr_handle.h"
#include "oxr_extension_support.h"
#include "oxr_subaction.h"
#include "oxr_chain.h"

#include <sys/types.h>
#ifdef XRT_OS_UNIX
#include <unistd.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

DEBUG_GET_ONCE_BOOL_OPTION(debug_views, "OXR_DEBUG_VIEWS", false)
DEBUG_GET_ONCE_BOOL_OPTION(debug_spaces, "OXR_DEBUG_SPACES", false)
DEBUG_GET_ONCE_BOOL_OPTION(debug_bindings, "OXR_DEBUG_BINDINGS", false)
DEBUG_GET_ONCE_BOOL_OPTION(lifecycle_verbose, "OXR_LIFECYCLE_VERBOSE", false)


static XrResult
oxr_instance_destroy(struct oxr_logger *log, struct oxr_handle_base *hb)
{
	struct oxr_instance *inst = (struct oxr_instance *)hb;

	// Does a null-ptr check.
	xrt_syscomp_destroy(&inst->system.xsysc);

	u_var_remove_root((void *)inst);

	oxr_binding_destroy_all(log, inst);

	oxr_path_destroy(log, inst);

	u_hashset_destroy(&inst->action_sets.name_store);
	u_hashset_destroy(&inst->action_sets.loc_store);

	// Free the mask here, no system destroy yet.
	for (uint32_t i = 0; i < ARRAY_SIZE(inst->system.visibility_mask); i++) {
		free(inst->system.visibility_mask[i]);
		inst->system.visibility_mask[i] = NULL;
	}

	xrt_space_overseer_destroy(&inst->system.xso);
	os_mutex_destroy(&inst->system.sync_actions_mutex);
	xrt_system_devices_destroy(&inst->system.xsysd);

#ifdef XRT_FEATURE_CLIENT_DEBUG_GUI
	u_debug_gui_stop(&inst->debug_ui);
#endif

	xrt_instance_destroy(&inst->xinst);

	// Does null checking and sets to null.
	time_state_destroy(&inst->timekeeping);

	// Mutex goes last.
	os_mutex_destroy(&inst->event.mutex);

	free(inst);

	return XR_SUCCESS;
}

static void
cache_path(struct oxr_logger *log, struct oxr_instance *inst, const char *str, XrPath *out_path)
{
	oxr_path_get_or_create(log, inst, str, strlen(str), out_path);
}

static bool
starts_with(const char *with, const char *string)
{
	assert(with != NULL);

	if (string == NULL) {
		return false;
	}

	for (uint32_t i = 0; with[i] != 0; i++) {
		if (string[i] != with[i]) {
			return false;
		}
	}

	return true;
}

static void
debug_print_devices(struct oxr_logger *log, struct oxr_system *sys)
{
#define D(INDEX) (roles.INDEX < 0 ? NULL : sys->xsysd->xdevs[roles.INDEX])
#define P(XDEV) (XDEV != NULL ? XDEV->str : "<none>")

	// Static roles.
	struct xrt_device *h = GET_XDEV_BY_ROLE(sys, head);
	struct xrt_device *e = GET_XDEV_BY_ROLE(sys, eyes);
	struct xrt_device *hl = GET_XDEV_BY_ROLE(sys, hand_tracking_left);
	struct xrt_device *hr = GET_XDEV_BY_ROLE(sys, hand_tracking_right);

	// Dynamic roles, the system cache might not have been updated yet.
	struct xrt_system_roles roles = XRT_SYSTEM_ROLES_INIT;
	xrt_system_devices_get_roles(sys->xsysd, &roles);

	struct xrt_device *l = D(left);
	struct xrt_device *r = D(right);
	struct xrt_device *gp = D(gamepad);

	oxr_log(log,
	        "Selected devices"
	        "\n\tHead: '%s'"
	        "\n\tEyes: '%s'"
	        "\n\tLeft: '%s'"
	        "\n\tRight: '%s'"
	        "\n\tGamepad: '%s'"
	        "\n\tHand-Tracking Left: '%s'"
	        "\n\tHand-Tracking Right: '%s'",
	        P(h), P(e), P(l), P(r), P(gp), P(hl), P(hr));

#undef P
#undef D
}

static void
detect_engine(struct oxr_logger *log, struct oxr_instance *inst, const XrInstanceCreateInfo *createInfo)
{
	if (starts_with("UnrealEngine4", createInfo->applicationInfo.engineName)) {
		inst->appinfo.detected.engine.name = "UnrealEngine";
		inst->appinfo.detected.engine.major = 4;
		inst->appinfo.detected.engine.minor = (createInfo->applicationInfo.engineVersion >> 16) & 0xffff;
		inst->appinfo.detected.engine.patch = createInfo->applicationInfo.engineVersion & 0xffff;
	}

	if (starts_with("UnrealEngine5", createInfo->applicationInfo.engineName)) {
		inst->appinfo.detected.engine.name = "UnrealEngine";
		inst->appinfo.detected.engine.major = 5;
		inst->appinfo.detected.engine.minor = (createInfo->applicationInfo.engineVersion >> 16) & 0xffff;
		inst->appinfo.detected.engine.patch = createInfo->applicationInfo.engineVersion & 0xffff;
	}
}

static void
apply_quirks(struct oxr_logger *log, struct oxr_instance *inst)
{
	// Reset.
	inst->quirks.skip_end_session = false;
	inst->quirks.disable_vulkan_format_depth_stencil = false;
	inst->quirks.no_validation_error_in_create_ref_space = false;

	if (starts_with("UnrealEngine", inst->appinfo.detected.engine.name) && //
	    inst->appinfo.detected.engine.major == 4 &&                        //
	    inst->appinfo.detected.engine.minor <= 27) {
		inst->quirks.skip_end_session = true;
	}

	// Currently always true.
	inst->quirks.no_validation_error_in_create_ref_space = true;
}

XrResult
oxr_instance_create(struct oxr_logger *log,
                    const XrInstanceCreateInfo *createInfo,
                    const struct oxr_extension_status *extensions,
                    struct oxr_instance **out_instance)
{
	struct oxr_instance *inst = NULL;
	int m_ret;
	int h_ret;
	xrt_result_t xret;
	XrResult ret;

	OXR_ALLOCATE_HANDLE_OR_RETURN(log, inst, OXR_XR_DEBUG_INSTANCE, oxr_instance_destroy, NULL);

	inst->extensions = *extensions; // Sets the enabled extensions.
	inst->lifecycle_verbose = debug_get_bool_option_lifecycle_verbose();
	inst->debug_spaces = debug_get_bool_option_debug_spaces();
	inst->debug_views = debug_get_bool_option_debug_views();
	inst->debug_bindings = debug_get_bool_option_debug_bindings();

	m_ret = os_mutex_init(&inst->event.mutex);
	if (m_ret < 0) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Failed to init mutex");
		return ret;
	}

	m_ret = os_mutex_init(&inst->system.sync_actions_mutex);
	if (m_ret < 0) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Failed to init sync action mutex");
		return ret;
	}

#ifdef XRT_FEATURE_CLIENT_DEBUG_GUI
	u_debug_gui_create(&inst->debug_ui);
#endif

	ret = oxr_path_init(log, inst);
	if (ret != XR_SUCCESS) {
		return ret;
	}

	h_ret = u_hashset_create(&inst->action_sets.name_store);
	if (h_ret != 0) {
		oxr_instance_destroy(log, &inst->handle);
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Failed to create name_store hashset");
	}

	h_ret = u_hashset_create(&inst->action_sets.loc_store);
	if (h_ret != 0) {
		oxr_instance_destroy(log, &inst->handle);
		return oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Failed to create loc_store hashset");
	}


	// Cache certain often looked up paths.


#define CACHE_SUBACTION_PATHS(NAME, NAME_CAPS, PATH) cache_path(log, inst, PATH, &inst->path_cache.NAME);
	OXR_FOR_EACH_SUBACTION_PATH_DETAILED(CACHE_SUBACTION_PATHS)

#undef CACHE_SUBACTION_PATHS
	// clang-format off

	cache_path(log, inst, "/interaction_profiles/khr/simple_controller", &inst->path_cache.khr_simple_controller);
	cache_path(log, inst, "/interaction_profiles/google/daydream_controller", &inst->path_cache.google_daydream_controller);
	cache_path(log, inst, "/interaction_profiles/htc/vive_controller", &inst->path_cache.htc_vive_controller);
	cache_path(log, inst, "/interaction_profiles/htc/vive_pro", &inst->path_cache.htc_vive_pro);
	cache_path(log, inst, "/interaction_profiles/microsoft/motion_controller", &inst->path_cache.microsoft_motion_controller);
	cache_path(log, inst, "/interaction_profiles/microsoft/xbox_controller", &inst->path_cache.microsoft_xbox_controller);
	cache_path(log, inst, "/interaction_profiles/oculus/go_controller", &inst->path_cache.oculus_go_controller);
	cache_path(log, inst, "/interaction_profiles/oculus/touch_controller", &inst->path_cache.oculus_touch_controller);
	cache_path(log, inst, "/interaction_profiles/valve/index_controller", &inst->path_cache.valve_index_controller);
	cache_path(log, inst, "/interaction_profiles/hp/mixed_reality_controller", &inst->path_cache.hp_mixed_reality_controller);
	cache_path(log, inst, "/interaction_profiles/samsung/odyssey_controller", &inst->path_cache.samsung_odyssey_controller);
	cache_path(log, inst, "/interaction_profiles/ml/ml2_controller", &inst->path_cache.ml_ml2_controller);
	cache_path(log, inst, "/interaction_profiles/mndx/ball_on_a_stick_controller", &inst->path_cache.mndx_ball_on_a_stick_controller);
	cache_path(log, inst, "/interaction_profiles/microsoft/hand_interaction", &inst->path_cache.msft_hand_interaction);
	cache_path(log, inst, "/interaction_profiles/ext/eye_gaze_interaction", &inst->path_cache.ext_eye_gaze_interaction);
	cache_path(log, inst, "/interaction_profiles/ext/hand_interaction_ext", &inst->path_cache.ext_hand_interaction);
	cache_path(log, inst, "/interaction_profiles/oppo/mr_controller_oppo", &inst->path_cache.oppo_mr_controller);

	// clang-format on

	// fill in our application info - @todo - replicate all createInfo
	// fields?

	struct xrt_instance_info i_info = {
	    .ext_hand_tracking_enabled = extensions->EXT_hand_tracking,
#ifdef OXR_HAVE_EXT_eye_gaze_interaction
	    .ext_eye_gaze_interaction_enabled = extensions->EXT_eye_gaze_interaction,
#endif
#ifdef OXR_HAVE_EXT_hand_interaction
	    .ext_hand_interaction_enabled = extensions->EXT_hand_interaction,
#endif
#ifdef OXR_HAVE_HTC_facial_tracking
	    .htc_facial_tracking_enabled = extensions->HTC_facial_tracking,
#endif
#ifdef OXR_HAVE_FB_body_tracking
	    .fb_body_tracking_enabled = extensions->FB_body_tracking,
#endif
#ifdef OXR_HAVE_META_body_tracking_full_body
	    .meta_body_tracking_full_body_enabled = extensions->META_body_tracking_full_body,
#endif
#ifdef OXR_HAVE_META_body_tracking_fidelity
	    .meta_body_tracking_fidelity_enabled = extensions->META_body_tracking_fidelity,
#endif
#ifdef OXR_HAVE_META_body_tracking_calibration
	    .meta_body_tracking_calibration_enabled = extensions->META_body_tracking_calibration,
#endif
	};
	snprintf(i_info.application_name, sizeof(inst->xinst->instance_info.application_name), "%s",
	         createInfo->applicationInfo.applicationName);

#ifdef XRT_OS_ANDROID
	XrInstanceCreateInfoAndroidKHR const *create_info_android = OXR_GET_INPUT_FROM_CHAIN(
	    createInfo, XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR, XrInstanceCreateInfoAndroidKHR);
	android_globals_store_vm_and_activity((struct _JavaVM *)create_info_android->applicationVM,
	                                      create_info_android->applicationActivity);
	// Trick to avoid deadlock on main thread. Only works for NativeActivity with app-glue.
	android_looper_poll_until_activity_resumed();
#endif


	/*
	 * Monado initialisation.
	 */

	xret = xrt_instance_create(&i_info, &inst->xinst);
	if (xret != XRT_SUCCESS) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Failed to create instance '%i'", xret);
		oxr_instance_destroy(log, &inst->handle);
		return ret;
	}

	struct oxr_system *sys = &inst->system;

	// Create the compositor if we are not headless, currently always create it.
	bool should_create_compositor = true /* !inst->extensions.MND_headless */;

	// Create the system.
	if (should_create_compositor) {
		xret = xrt_instance_create_system(inst->xinst, &sys->xsys, &sys->xsysd, &sys->xso, &sys->xsysc);
	} else {
		xret = xrt_instance_create_system(inst->xinst, &sys->xsys, &sys->xsysd, &sys->xso, NULL);
	}

	if (xret != XRT_SUCCESS) {
		ret = oxr_error(log, XR_ERROR_INITIALIZATION_FAILED, "Failed to create the system '%i'", xret);
		oxr_instance_destroy(log, &inst->handle);
		return ret;
	}

	ret = XR_SUCCESS;
	if (sys->xsysd == NULL) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Huh?! Field sys->xsysd was NULL?");
	} else if (should_create_compositor && sys->xsysc == NULL) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Huh?! Field sys->xsysc was NULL?");
	} else if (!should_create_compositor && sys->xsysc != NULL) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Huh?! Field sys->xsysc was not NULL?");
	}

	if (ret != XR_SUCCESS) {
		oxr_instance_destroy(log, &inst->handle);
		return ret;
	}

	// Did we find any HMD
	// @todo Headless with only controllers?
	struct xrt_device *dev = GET_XDEV_BY_ROLE(sys, head);
	if (dev == NULL) {
		ret = oxr_error(log, XR_ERROR_RUNTIME_FAILURE, "Failed to find any HMD device");
		oxr_instance_destroy(log, &inst->handle);
		return ret;
	}
	uint32_t view_count = dev->hmd->view_count;
	ret = oxr_system_fill_in(log, inst, XRT_SYSTEM_ID, view_count, &inst->system);
	if (ret != XR_SUCCESS) {
		oxr_instance_destroy(log, &inst->handle);
		return ret;
	}

	inst->timekeeping = time_state_create(inst->xinst->startup_timestamp);

	//! @todo check if this (and other creates) failed?

	// Detect game engine.
	detect_engine(log, inst, createInfo);

	// Apply any quirks
	apply_quirks(log, inst);

	u_var_add_root((void *)inst, "XrInstance", true);

#ifdef XRT_FEATURE_CLIENT_DEBUG_GUI
	u_debug_gui_start(inst->debug_ui, inst->xinst, sys->xsysd);
#endif

	oxr_log(log,
	        "Instance created\n"
	        "\tcreateInfo->applicationInfo.applicationName: %s\n"
	        "\tcreateInfo->applicationInfo.applicationVersion: %i\n"
	        "\tcreateInfo->applicationInfo.engineName: %s\n"
	        "\tcreateInfo->applicationInfo.engineVersion: %i\n"
	        "\tappinfo.detected.engine.name: %s\n"
	        "\tappinfo.detected.engine.version: %i.%i.%i\n"
	        "\tquirks.disable_vulkan_format_depth_stencil: %s\n"
	        "\tquirks.no_validation_error_in_create_ref_space: %s",
	        createInfo->applicationInfo.applicationName,                              //
	        createInfo->applicationInfo.applicationVersion,                           //
	        createInfo->applicationInfo.engineName,                                   //
	        createInfo->applicationInfo.engineVersion,                                //
	        inst->appinfo.detected.engine.name,                                       //
	        inst->appinfo.detected.engine.major,                                      //
	        inst->appinfo.detected.engine.minor,                                      //
	        inst->appinfo.detected.engine.patch,                                      //
	        inst->quirks.disable_vulkan_format_depth_stencil ? "true" : "false",      //
	        inst->quirks.no_validation_error_in_create_ref_space ? "true" : "false"); //

	debug_print_devices(log, sys);


#ifdef XRT_FEATURE_RENDERDOC

#ifdef __GNUC__
// Keep the warnings about normal usage of dlsym away.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif // __GNUC_

#if defined(XRT_OS_LINUX) && !defined(XRT_OS_ANDROID)
	void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
	if (mod) {
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
		XRT_MAYBE_UNUSED int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **)&inst->rdoc_api);
		assert(ret == 1);
	}
#endif
#ifdef XRT_OS_ANDROID
	void *mod = dlopen("libVkLayer_GLES_RenderDoc.so", RTLD_NOW | RTLD_NOLOAD);
	if (mod) {
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **)&inst->rdoc_api);
		assert(ret == 1);
	}
#endif
#ifdef XRT_OS_WINDOWS
	HMODULE mod = GetModuleHandleA("renderdoc.dll");
	if (mod) {
		pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
		int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_5_0, (void **)&inst->rdoc_api);
		assert(ret == 1);
	}
#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif // __GNUC_

#endif

	*out_instance = inst;

	return XR_SUCCESS;
}


XrResult
oxr_instance_get_properties(struct oxr_logger *log, struct oxr_instance *inst, XrInstanceProperties *instanceProperties)
{
	instanceProperties->runtimeVersion = XR_MAKE_VERSION(u_version_major, u_version_minor, u_version_patch);
	snprintf(instanceProperties->runtimeName, XR_MAX_RUNTIME_NAME_SIZE - 1, "%s '%s'", u_runtime_description,
	         u_git_tag);

	return XR_SUCCESS;
}

#ifdef XR_USE_TIMESPEC

XrResult
oxr_instance_convert_time_to_timespec(struct oxr_logger *log,
                                      struct oxr_instance *inst,
                                      XrTime time,
                                      struct timespec *timespecTime)
{
	time_state_to_timespec(inst->timekeeping, time, timespecTime);
	return XR_SUCCESS;
}

XrResult
oxr_instance_convert_timespec_to_time(struct oxr_logger *log,
                                      struct oxr_instance *inst,
                                      const struct timespec *timespecTime,
                                      XrTime *time)
{
	*time = time_state_from_timespec(inst->timekeeping, timespecTime);
	return XR_SUCCESS;
}
#endif // XR_USE_TIMESPEC

#ifdef XR_USE_PLATFORM_WIN32

XrResult
oxr_instance_convert_time_to_win32perfcounter(struct oxr_logger *log,
                                              struct oxr_instance *inst,
                                              XrTime time,
                                              LARGE_INTEGER *win32perfcounterTime)
{
	time_state_to_win32perfcounter(inst->timekeeping, time, win32perfcounterTime);
	return XR_SUCCESS;
}

XrResult
oxr_instance_convert_win32perfcounter_to_time(struct oxr_logger *log,
                                              struct oxr_instance *inst,
                                              const LARGE_INTEGER *win32perfcounterTime,
                                              XrTime *time)
{
	*time = time_state_from_win32perfcounter(inst->timekeeping, win32perfcounterTime);
	return XR_SUCCESS;
}

#endif // XR_USE_PLATFORM_WIN32

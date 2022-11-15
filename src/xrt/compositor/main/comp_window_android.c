// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Android window code.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup comp_main
 */

#include "xrt/xrt_compiler.h"
#include "xrt/xrt_config_build.h"
#include "xrt/xrt_instance.h"
#include "xrt/xrt_android.h"

#include "util/u_misc.h"
#include "os/os_threading.h"

#include "android/android_globals.h"
#include "android/android_custom_surface.h"

#include "main/comp_window.h"

#include <android/native_window.h>

#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>


/*
 *
 * Private structs.
 *
 */

/*!
 * An Android window.
 *
 * @implements comp_target_swapchain
 */
struct comp_window_android
{
	struct comp_target_swapchain base;

	uint32_t width;
	uint32_t height;
	VkFormat color_format;
	VkColorSpaceKHR color_space;
	VkPresentModeKHR present_mode;
	void (*real_create_images)(struct comp_target *ct,
	                           uint32_t preferred_width,
	                           uint32_t preferred_height,
	                           VkFormat preferred_color_format,
	                           VkColorSpaceKHR preferred_color_space,
	                           VkPresentModeKHR present_mode);

	bool needs_create_images;

	ANativeWindow *native_window;
	struct os_mutex surface_mutex;

	struct android_custom_surface *custom_surface;
};


/*
 *
 * Functions.
 *
 */

static inline struct vk_bundle *
get_vk(struct comp_window_android *cwa)
{
	return &cwa->base.base.c->base.vk;
}

static bool
comp_window_android_init_pre_vulkan(struct comp_target *ct)
{
#ifdef XRT_FEATURE_SERVICE
	(void)ct;
#else
	struct comp_window_android *cwa = (struct comp_window_android *)ct;

	if (android_globals_get_activity() == NULL) {
		COMP_ERROR(cwa->base.base.c,
		           "comp_window_android_init_pre_vulkan: could not "
		           "find our activity to attach the custom surface");
		return false;
	}

	cwa->custom_surface =
	    android_custom_surface_async_start(android_globals_get_vm(), android_globals_get_activity());
	if (cwa->custom_surface == NULL) {
		COMP_ERROR(cwa->base.base.c,
		           "comp_window_android_init_pre_vulkan: could not "
		           "start asynchronous attachment of our custom surface");
		return false;
	}

#endif
	return true;
}


static void
comp_window_android_update_window_title(struct comp_target *ct, const char *title)
{
	(void)ct;
}

static VkResult
comp_window_android_create_surface(struct comp_window_android *cwa,
                                   struct ANativeWindow *window,
                                   VkSurfaceKHR *vk_surface)
{
	struct vk_bundle *vk = get_vk(cwa);
	VkResult ret;

	VkAndroidSurfaceCreateInfoKHR surface_info = {
	    .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
	    .flags = 0,
	    .window = window,
	};

	ret = vk->vkCreateAndroidSurfaceKHR( //
	    vk->instance,                    //
	    &surface_info,                   //
	    NULL,                            //
	    vk_surface);                     //
	if (ret != VK_SUCCESS) {
		COMP_ERROR(cwa->base.base.c, "vkCreateAndroidSurfaceKHR: %s", vk_result_string(ret));
		return ret;
	}

	return VK_SUCCESS;
}

static bool
comp_window_android_init_post_vulkan(struct comp_target *ct, uint32_t width, uint32_t height)
{
	struct comp_window_android *cwa = (struct comp_window_android *)ct;
	VkResult ret;

	cwa->width = width;
	cwa->height = height;

#ifdef XRT_FEATURE_SERVICE
	/* Out of process: Getting cached surface */
	ANativeWindow *window = (ANativeWindow *)android_globals_get_window();
#else
	ANativeWindow *window = android_custom_surface_wait_get_surface(cwa->custom_surface, 2000);
#endif

	if (window == NULL) {
		COMP_ERROR(cwa->base.base.c, "could not get ANativeWindow");
		return false;
	}

	ret = comp_window_android_create_surface(cwa, window, &cwa->base.surface.handle);
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "Failed to create surface '%s'!", vk_result_string(ret));
		return false;
	}

	return true;
}

static void
comp_window_android_flush(struct comp_target *ct)
{
	(void)ct;
}

static void
comp_window_android_create_images_stub(struct comp_target *ct,
                                       uint32_t width,
                                       uint32_t height,
                                       VkFormat color_format,
                                       VkColorSpaceKHR color_space,
                                       VkPresentModeKHR present_mode)
{

	struct comp_window_android *cwa = (struct comp_window_android *)ct;
	if (cwa->native_window != NULL) {
		cwa->real_create_images(ct, width, height, color_format, color_space, present_mode);
		return;
	}
	cwa->width = width;
	cwa->height = height;
	cwa->color_format = color_format;
	cwa->color_space = color_space;
	cwa->present_mode = present_mode;
	cwa->needs_create_images = true;
}

static bool
comp_window_android_handle_surface_acquired(struct xrt_instance *xinst,
                                            struct _ANativeWindow *window,
                                            enum xrt_android_surface_event event,
                                            void *userdata)
{
	struct comp_window_android *cwa = (struct comp_window_android *)userdata;
	COMP_INFO(cwa->base.base.c, "comp_window_android_handle_surface_acquired: got a surface!");
	if (cwa->native_window == NULL) {
		cwa->native_window = (ANativeWindow *)window;
		VkResult ret = comp_window_android_create_surface(cwa, cwa->native_window, &cwa->base.surface.handle);
		if (ret != VK_SUCCESS) {
			COMP_ERROR(cwa->base.base.c, "Failed to create surface '%s'!", vk_result_string(ret));
			return true;
		}
		if (cwa->needs_create_images) {
			cwa->needs_create_images = false;
			cwa->real_create_images(&cwa->base.base, cwa->width, cwa->height, cwa->color_format,
			                        cwa->color_space, cwa->present_mode);
		}
	}
	return true;
}

static bool
comp_window_android_handle_surface_lost(struct xrt_instance *xinst,
                                        struct _ANativeWindow *window,
                                        enum xrt_android_surface_event event,
                                        void *userdata)
{
	struct comp_window_android *cwa = (struct comp_window_android *)userdata;
	COMP_INFO(cwa->base.base.c, "comp_window_android_handle_surface_lost: oh noes!");
	if (cwa->native_window == (ANativeWindow *)window) {
		// yeah, we're losing this surface.
		os_mutex_lock(&cwa->surface_mutex);

		comp_target_swapchain_cleanup(&cwa->base);
		cwa->native_window = NULL;

		os_mutex_unlock(&cwa->surface_mutex);
	}
	return true;
}

static void
comp_window_android_destroy(struct comp_target *ct)
{
	struct comp_window_android *cwa = (struct comp_window_android *)ct;
	struct xrt_instance *xinst = cwa->base.base.c->xinst;
	xinst->android_instance->remove_surface_callback(xinst, comp_window_android_handle_surface_acquired,
	                                                 XRT_ANDROID_SURFACE_EVENT_ACQUIRED, (void *)cwa);
	xinst->android_instance->remove_surface_callback(xinst, comp_window_android_handle_surface_lost,
	                                                 XRT_ANDROID_SURFACE_EVENT_LOST, (void *)cwa);

	os_mutex_destroy(&cwa->surface_mutex);
	comp_target_swapchain_cleanup(&cwa->base);

	android_custom_surface_destroy(&cwa->custom_surface);

	free(ct);
}

struct comp_target *
comp_window_android_create(struct comp_compositor *c)
{
	struct comp_window_android *cwa = U_TYPED_CALLOC(struct comp_window_android);

	// The display timing code hasn't been tested on Android and may be broken.
	comp_target_swapchain_init_and_set_fnptrs(&cwa->base, COMP_TARGET_FORCE_FAKE_DISPLAY_TIMING);

	cwa->base.base.name = "Android";
	cwa->base.base.destroy = comp_window_android_destroy;
	cwa->base.base.flush = comp_window_android_flush;
	cwa->base.base.init_pre_vulkan = comp_window_android_init_pre_vulkan;
	cwa->base.base.init_post_vulkan = comp_window_android_init_post_vulkan;
	cwa->base.base.set_title = comp_window_android_update_window_title;
	cwa->base.base.c = c;

	// Intercept this call
	cwa->real_create_images = cwa->base.base.create_images;
	cwa->base.base.create_images = comp_window_android_create_images_stub;

	os_mutex_init(&cwa->surface_mutex);

	struct xrt_instance *xinst = c->xinst;
	xinst->android_instance->register_surface_callback(xinst, comp_window_android_handle_surface_acquired,
	                                                   XRT_ANDROID_SURFACE_EVENT_ACQUIRED, (void *)cwa);
	xinst->android_instance->register_surface_callback(xinst, comp_window_android_handle_surface_lost,
	                                                   XRT_ANDROID_SURFACE_EVENT_LOST, (void *)cwa);

	return &cwa->base.base;
}

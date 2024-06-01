// Copyright 2019-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Direct mode window code.
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup comp_main
 */

#include "util/u_misc.h"
#include "util/u_pretty_print.h"

#include "main/comp_window_direct.h"


/*
 *
 * Private structs and defines.
 *
 */

//! NVIDIA Vendor ID.
#define NVIDIA_VENDOR_ID (0x10DE)

/*!
 * Probed display.
 */
struct comp_window_direct_nvidia_display
{
	char *name;
	VkDisplayPropertiesKHR display_properties;
	VkDisplayKHR display;
};

/*!
 * Direct mode "window" into a device, using Vulkan direct mode extension
 * and xcb.
 *
 * @implements comp_target_swapchain
 */
struct comp_window_direct_nvidia
{
	struct comp_target_swapchain base;

	Display *dpy;
	struct comp_window_direct_nvidia_display *displays;
	uint16_t display_count;
};


/*
 *
 * Forward declare functions
 *
 */

static void
comp_window_direct_nvidia_destroy(struct comp_target *ct);

static bool
comp_window_direct_nvidia_init(struct comp_target *ct);

static struct comp_window_direct_nvidia_display *
comp_window_direct_nvidia_current_display(struct comp_window_direct_nvidia *w);

static bool
comp_window_direct_nvidia_init_swapchain(struct comp_target *ct, uint32_t width, uint32_t height);


/*
 *
 * Functions.
 *
 */

static inline struct vk_bundle *
get_vk(struct comp_target *ct)
{
	return &ct->c->base.vk;
}

static void
_flush(struct comp_target *ct)
{
	(void)ct;
}

static void
_update_window_title(struct comp_target *ct, const char *title)
{
	(void)ct;
	(void)title;
}

struct comp_target *
comp_window_direct_nvidia_create(struct comp_compositor *c)
{
	struct comp_window_direct_nvidia *w = U_TYPED_CALLOC(struct comp_window_direct_nvidia);

	// The display timing code hasn't been tested on nVidia and may be broken.
	comp_target_swapchain_init_and_set_fnptrs(&w->base, COMP_TARGET_FORCE_FAKE_DISPLAY_TIMING);

	w->base.base.name = "direct";
	w->base.display = VK_NULL_HANDLE;
	w->base.base.destroy = comp_window_direct_nvidia_destroy;
	w->base.base.flush = _flush;
	w->base.base.init_pre_vulkan = comp_window_direct_nvidia_init;
	w->base.base.init_post_vulkan = comp_window_direct_nvidia_init_swapchain;
	w->base.base.set_title = _update_window_title;
	w->base.base.c = c;

	return &w->base.base;
}

static void
comp_window_direct_nvidia_destroy(struct comp_target *ct)
{
	struct comp_window_direct_nvidia *w_direct = (struct comp_window_direct_nvidia *)ct;

	comp_target_swapchain_cleanup(&w_direct->base);

	for (uint32_t i = 0; i < w_direct->display_count; i++) {
		struct comp_window_direct_nvidia_display *d = &w_direct->displays[i];
		d->display = VK_NULL_HANDLE;
		free(d->name);
	}

	if (w_direct->displays != NULL)
		free(w_direct->displays);

	if (w_direct->dpy) {
		XCloseDisplay(w_direct->dpy);
		w_direct->dpy = NULL;
	}

	free(ct);
}

static bool
append_nvidia_entry_on_match(struct comp_window_direct_nvidia *w,
                             const char *wl_entry,
                             struct VkDisplayPropertiesKHR *disp)
{
	unsigned long wl_entry_length = strlen(wl_entry);
	unsigned long disp_entry_length = strlen(disp->displayName);

	// If the entry is shorter then it will never match.
	if (disp_entry_length < wl_entry_length) {
		return false;
	}

	// We only check the first part of the string, extra characters ignored.
	if (strncmp(wl_entry, disp->displayName, wl_entry_length) != 0) {
		return false;
	}

	/*
	 * We have a match with this allow list entry.
	 */

	// Make the compositor use this size.
	comp_target_swapchain_override_extents(&w->base, disp->physicalResolution);

	// Create the entry.
	struct comp_window_direct_nvidia_display d = {
	    .name = U_TYPED_ARRAY_CALLOC(char, disp_entry_length + 1),
	    .display_properties = *disp,
	    .display = disp->display,
	};

	memcpy(d.name, disp->displayName, disp_entry_length);
	d.name[disp_entry_length] = '\0';

	w->display_count += 1;

	U_ARRAY_REALLOC_OR_FREE(w->displays, struct comp_window_direct_nvidia_display, w->display_count);

	if (w->displays == NULL) {
		COMP_ERROR(w->base.base.c, "Unable to reallocate NVIDIA displays");

		// Reset the count.
		w->display_count = 0;
		return false;
	}

	w->displays[w->display_count - 1] = d;

	return true;
}

static bool
comp_window_direct_nvidia_init(struct comp_target *ct)
{
	struct comp_window_direct_nvidia *w_direct = (struct comp_window_direct_nvidia *)ct;
	struct vk_bundle *vk = get_vk(ct);
	VkDisplayPropertiesKHR *display_props = NULL;
	uint32_t display_count = 0;
	VkResult ret;

	if (vk->instance == VK_NULL_HANDLE) {
		COMP_ERROR(ct->c, "Vulkan not initialized before NVIDIA init!");
		return false;
	}

	if (!comp_window_direct_connect(&w_direct->base, &w_direct->dpy)) {
		return false;
	}

	// find our display using nvidia allowlist, enumerate its modes, and
	// pick the best one get a list of attached displays

	ret = vk_enumerate_physical_device_display_properties( //
	    vk,                                                //
	    vk->physical_device,                               //
	    &display_count,                                    //
	    &display_props);                                   //
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "vk_enumerate_physical_device_display_properties: %s", vk_result_string(ret));
		return false;
	}

	if (display_count == 0) {
		COMP_ERROR(ct->c, "NVIDIA: No Vulkan displays found.");
		return false;
	}

	/// @todo what if we have multiple allowlisted HMD displays connected?
	for (uint32_t i = 0; i < display_count; i++) {
		struct VkDisplayPropertiesKHR disp = *(display_props + i);

		if (ct->c->settings.nvidia_display) {
			append_nvidia_entry_on_match(w_direct, ct->c->settings.nvidia_display, &disp);
		}

		// check this display against our allowlist
		for (uint32_t j = 0; j < ARRAY_SIZE(NV_DIRECT_ALLOWLIST); j++)
			if (append_nvidia_entry_on_match(w_direct, NV_DIRECT_ALLOWLIST[j], &disp))
				break;
	}

	free(display_props);

	return true;
}

static struct comp_window_direct_nvidia_display *
comp_window_direct_nvidia_current_display(struct comp_window_direct_nvidia *w)
{
	int index = w->base.base.c->settings.display;
	if (index == -1)
		index = 0;

	if (w->display_count <= (uint32_t)index)
		return NULL;

	return &w->displays[index];
}

static bool
comp_window_direct_nvidia_init_swapchain(struct comp_target *ct, uint32_t width, uint32_t height)
{
	struct comp_window_direct_nvidia *w_direct = (struct comp_window_direct_nvidia *)ct;

	struct comp_window_direct_nvidia_display *d = comp_window_direct_nvidia_current_display(w_direct);
	if (!d) {
		COMP_ERROR(ct->c, "NVIDIA could not find any HMDs.");
		return false;
	}

	COMP_DEBUG(ct->c, "Will use display: %s", d->name);
	struct comp_target_swapchain *cts = (struct comp_target_swapchain *)ct;
	cts->display = d->display;

	return comp_window_direct_init_swapchain(&w_direct->base, w_direct->dpy, d->display, width, height);
}


/*
 *
 * Factory
 *
 */

static const char *instance_extensions[] = {
    VK_KHR_DISPLAY_EXTENSION_NAME,
    VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
    VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME,
};

static bool
_match_allowlist_entry(const char *al_entry, VkDisplayPropertiesKHR *disp)
{
	unsigned long al_entry_length = strlen(al_entry);
	unsigned long disp_entry_length = strlen(disp->displayName);
	if (disp_entry_length < al_entry_length)
		return false;

	// we have a match with this allowlist entry.
	if (strncmp(al_entry, disp->displayName, al_entry_length) == 0)
		return true;

	return false;
}

/*
 * our physical device is an nvidia card, we can potentially select
 * nvidia-specific direct mode.
 *
 * we need to also check if we are confident that we can create a direct mode
 * display, if not we need to abandon the attempt here, and allow desktop-window
 * fallback to occur.
 */

static bool
_test_for_nvidia(struct comp_compositor *c, struct vk_bundle *vk)
{
	VkDisplayPropertiesKHR *display_props;
	uint32_t display_count;
	VkResult ret;

	VkPhysicalDeviceProperties physical_device_properties;
	vk->vkGetPhysicalDeviceProperties(vk->physical_device, &physical_device_properties);

	// Only run this code on NVIDIA hardware.
	if (physical_device_properties.vendorID != NVIDIA_VENDOR_ID) {
		return false;
	}

	// Get a list of attached displays.
	ret = vk_enumerate_physical_device_display_properties( //
	    vk,                                                //
	    vk->physical_device,                               //
	    &display_count,                                    //
	    &display_props);                                   //
	if (ret != VK_SUCCESS) {
		CVK_ERROR(c, "vk_enumerate_physical_device_display_properties", "Failed to get display properties ",
		          ret);
		return false;
	}

	for (uint32_t i = 0; i < display_count; i++) {
		VkDisplayPropertiesKHR *disp = display_props + i;

		// Check this display against our allowlist.
		for (uint32_t j = 0; j < ARRAY_SIZE(NV_DIRECT_ALLOWLIST); j++) {
			if (_match_allowlist_entry(NV_DIRECT_ALLOWLIST[j], disp)) {
				free(display_props);
				return true;
			}
		}

		// Also check against any extra displays given by the user.
		if (c->settings.nvidia_display && _match_allowlist_entry(c->settings.nvidia_display, disp)) {
			free(display_props);
			return true;
		}
	}

	struct u_pp_sink_stack_only sink;
	u_pp_delegate_t dg = u_pp_sink_stack_only_init(&sink);

	u_pp(dg, "NVIDIA: No allowlisted displays found!");

	u_pp(dg, "\n\t== Current Allowlist (%u) ==", (uint32_t)ARRAY_SIZE(NV_DIRECT_ALLOWLIST));
	for (uint32_t i = 0; i < ARRAY_SIZE(NV_DIRECT_ALLOWLIST); i++) {
		u_pp(dg, "\n\t\t%s", NV_DIRECT_ALLOWLIST[i]);
	}

	if (c->settings.nvidia_display != NULL) {
		u_pp(dg, "\n\t\t%s (extra)", c->settings.nvidia_display);
	}

	u_pp(dg, "\n\t== Found Displays (%u) ==", display_count);
	for (uint32_t i = 0; i < display_count; i++) {
		u_pp(dg, "\n\t\t%s", display_props[i].displayName);
	}

	COMP_ERROR(c, "%s", sink.buffer);

	free(display_props);

	return false;
}

static bool
check_vulkan_caps(struct comp_compositor *c, bool *out_detected)
{
	VkResult ret;

	*out_detected = false;

	// this is duplicative, but seems to be the easiest way to
	// 'pre-check' capabilities when window creation precedes vulkan
	// instance creation. we also need to load the VK_KHR_DISPLAY
	// extension.

	COMP_DEBUG(c, "Checking for NVIDIA vulkan driver.");

	struct vk_bundle temp_vk_storage = {0};
	struct vk_bundle *temp_vk = &temp_vk_storage;
	temp_vk->log_level = U_LOGGING_WARN;

	ret = vk_get_loader_functions(temp_vk, vkGetInstanceProcAddr);
	if (ret != VK_SUCCESS) {
		CVK_ERROR(c, "vk_get_loader_functions", "Failed to get loader functions.", ret);
		return false;
	}

	const char *extension_names[] = {
	    COMP_INSTANCE_EXTENSIONS_COMMON,
	    VK_KHR_DISPLAY_EXTENSION_NAME,
	};

	VkInstanceCreateInfo instance_create_info = {
	    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
	    .enabledExtensionCount = ARRAY_SIZE(extension_names),
	    .ppEnabledExtensionNames = extension_names,
	};

	ret = temp_vk->vkCreateInstance(&instance_create_info, NULL, &(temp_vk->instance));
	if (ret != VK_SUCCESS) {
		CVK_ERROR(c, "vkCreateInstance", "Failed to create VkInstance.", ret);
		return false;
	}

	ret = vk_get_instance_functions(temp_vk);
	if (ret != VK_SUCCESS) {
		CVK_ERROR(c, "vk_get_instance_functions", "Failed to get Vulkan instance functions.", ret);
		return false;
	}

	ret = vk_select_physical_device(temp_vk, c->settings.selected_gpu_index, false);
	if (ret != VK_SUCCESS) {
		CVK_ERROR(c, "vk_select_physical_device", "Failed to select physical device.", ret);
		return false;
	}

	if (_test_for_nvidia(c, temp_vk)) {
		*out_detected = true;
		COMP_DEBUG(c, "Selecting direct NVIDIA window type!");
	}

	temp_vk->vkDestroyInstance(temp_vk->instance, NULL);

	return true;
}

static bool
detect(const struct comp_target_factory *ctf, struct comp_compositor *c)
{
	bool detected = false;

	if (!check_vulkan_caps(c, &detected)) {
		return false;
	}

	return detected;
}

static bool
create_target(const struct comp_target_factory *ctf, struct comp_compositor *c, struct comp_target **out_ct)
{
	struct comp_target *ct = comp_window_direct_nvidia_create(c);
	if (ct == NULL) {
		return false;
	}

	*out_ct = ct;

	return true;
}

const struct comp_target_factory comp_target_factory_direct_nvidia = {
    .name = "NVIDIA Direct-Mode",
    .identifier = "x11_direct_nvidia",
    .requires_vulkan_for_create = true,
    .is_deferred = false,
    .required_instance_version = 0,
    .required_instance_extensions = instance_extensions,
    .required_instance_extension_count = ARRAY_SIZE(instance_extensions),
    .optional_device_extensions = NULL,
    .optional_device_extension_count = 0,
    .detect = detect,
    .create_target = create_target,
};

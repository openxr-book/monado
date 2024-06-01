// Copyright 2019-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Functions to init various parts of the vk_bundle.
 *
 * Note that some sections of this are generated
 * by `scripts/generate_vk_helpers.py` - lists of functions
 * and of optional extensions to check for. In those,
 * please update the script and run it, instead of editing
 * directly in this file. The generated parts are delimited
 * by special comments.
 *
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Moses Turner <moses@collabora.com>
 * @ingroup aux_vk
 */

#include "util/u_pretty_print.h"
#include "vk/vk_helpers.h"

#include <stdio.h>


/*
 *
 * Helpers.
 *
 */

static inline void
append_to_pnext_chain(VkBaseInStructure *head, VkBaseInStructure *new_struct)
{
	assert(new_struct->pNext == NULL);
	// Insert ourselves between head and its previous pNext
	new_struct->pNext = head->pNext;
	head->pNext = (void *)new_struct;
}

static bool
should_skip_optional_instance_ext(struct vk_bundle *vk,
                                  struct u_string_list *required_instance_ext_list,
                                  struct u_string_list *optional_instance_ext_listconst,
                                  const char *ext)
{
#ifdef VK_EXT_display_surface_counter
	if (strcmp(ext, VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME) == 0) {
		// it does not make sense to enable surface counter on anything that does not use a VkDisplayKHR
		if (!u_string_list_contains(required_instance_ext_list, VK_KHR_DISPLAY_EXTENSION_NAME)) {
			VK_DEBUG(vk, "Skipping optional instance extension %s because %s is not enabled", ext,
			         VK_KHR_DISPLAY_EXTENSION_NAME);
			return true;
		}
		VK_DEBUG(vk, "Not skipping optional instance extension %s because %s is enabled", ext,
		         VK_KHR_DISPLAY_EXTENSION_NAME);
	}
#endif
	return false;
}

static bool
is_instance_ext_supported(VkExtensionProperties *props, uint32_t prop_count, const char *ext)
{
	for (uint32_t j = 0; j < prop_count; j++) {
		if (strcmp(ext, props[j].extensionName) == 0) {
			return true;
		}
	}
	return false;
}


/*
 *
 * 'Exported' instance functions.
 *
 */

VkResult
vk_check_required_instance_extensions(struct vk_bundle *vk, struct u_string_list *required_instance_ext_list)
{
	struct u_pp_sink_stack_only sink;
	VkExtensionProperties *props = NULL;
	uint32_t prop_count = 0;
	VkResult ret;

	// Two call.
	ret = vk_enumerate_instance_extensions_properties( //
	    vk,                                            // vk_bundle
	    NULL,                                          // layer_name
	    &prop_count,                                   // out_prop_count
	    &props);                                       // out_props
	if (ret != VK_SUCCESS) {
		return ret; // Already logged.
	}

	// We want to print all missing extensions.
	bool have_missing = false;

	// Used to build a nice pretty list of missing extensions.
	u_pp_delegate_t dg = u_pp_sink_stack_only_init(&sink);

	// Check if required extensions are supported.
	uint32_t required_instance_ext_count = u_string_list_get_size(required_instance_ext_list);
	const char *const *required_instance_exts = u_string_list_get_data(required_instance_ext_list);
	for (uint32_t i = 0; i < required_instance_ext_count; i++) {
		const char *required_ext = required_instance_exts[i];

		if (is_instance_ext_supported(props, prop_count, required_ext)) {
			continue;
		}

		u_pp(dg, "\n\t%s", required_ext);
		have_missing = true;
	}

	// Clean up after us.
	if (props != NULL) {
		free(props);
		props = NULL;
		prop_count = 0;
	}

	if (!have_missing) {
		return VK_SUCCESS;
	}

	VK_ERROR(vk, "Missing required instance extensions:%s", sink.buffer);

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

struct u_string_list *
vk_build_instance_extensions(struct vk_bundle *vk,
                             struct u_string_list *required_instance_ext_list,
                             struct u_string_list *optional_instance_ext_list)
{
	VkExtensionProperties *props = NULL;
	uint32_t prop_count = 0;
	VkResult ret;

	// Two call.
	ret = vk_enumerate_instance_extensions_properties( //
	    vk,                                            // vk_bundle
	    NULL,                                          // layer_name
	    &prop_count,                                   // out_prop_count
	    &props);                                       // out_props
	if (ret != VK_SUCCESS) {
		return NULL; // Already logged.
	}

	// Assumed to be supported.
	struct u_string_list *list = u_string_list_create_from_list(required_instance_ext_list);

	// Check any supported extensions.
	uint32_t optional_instance_ext_count = u_string_list_get_size(optional_instance_ext_list);
	const char *const *optional_instance_exts = u_string_list_get_data(optional_instance_ext_list);
	for (uint32_t i = 0; i < optional_instance_ext_count; i++) {
		const char *optional_ext = optional_instance_exts[i];

		if (should_skip_optional_instance_ext(vk, required_instance_ext_list, optional_instance_ext_list,
		                                      optional_ext)) {
			continue;
		}

		if (!is_instance_ext_supported(props, prop_count, optional_ext)) {
			VK_DEBUG(vk, "Optional instance extension %s not enabled, unsupported", optional_ext);
			continue;
		}

		int added = u_string_list_append_unique(list, optional_ext);
		if (added == 1) {
			VK_DEBUG(vk, "Using optional instance ext %s", optional_ext);
		} else {
			VK_WARN(vk, "Duplicate instance extension %s not added twice", optional_ext);
		}
	}

	free(props);

	return list;
}

void
vk_fill_in_has_instance_extensions(struct vk_bundle *vk, struct u_string_list *ext_list)
{
	// beginning of GENERATED instance extension code - do not modify - used by scripts
	// Reset before filling out.
	vk->has_EXT_display_surface_counter = false;
	vk->has_EXT_swapchain_colorspace = false;
	vk->has_EXT_debug_utils = false;

	const char *const *exts = u_string_list_get_data(ext_list);
	uint32_t ext_count = u_string_list_get_size(ext_list);

	for (uint32_t i = 0; i < ext_count; i++) {
		const char *ext = exts[i];

#if defined(VK_EXT_display_surface_counter)
		if (strcmp(ext, VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME) == 0) {
			vk->has_EXT_display_surface_counter = true;
			continue;
		}
#endif // defined(VK_EXT_display_surface_counter)

#if defined(VK_EXT_swapchain_colorspace)
		if (strcmp(ext, VK_EXT_SWAPCHAIN_COLORSPACE_EXTENSION_NAME) == 0) {
			vk->has_EXT_swapchain_colorspace = true;
			continue;
		}
#endif // defined(VK_EXT_swapchain_colorspace)

#if defined(VK_EXT_debug_utils)
		if (strcmp(ext, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
			vk->has_EXT_debug_utils = true;
			continue;
		}
#endif // defined(VK_EXT_debug_utils)
	}
	// end of GENERATED instance extension code - do not modify - used by scripts
}


/*
 *
 * Physical device feature helpers.
 *
 */

static void
fill_in_device_features(struct vk_bundle *vk)
{
	/*
	 * Device properties.
	 */

	VkPhysicalDeviceProperties pdp;
	vk->vkGetPhysicalDeviceProperties(vk->physical_device, &pdp);

	vk->features.timestamp_compute_and_graphics = pdp.limits.timestampComputeAndGraphics;
	vk->features.timestamp_period = pdp.limits.timestampPeriod;
	vk->features.max_per_stage_descriptor_sampled_images = pdp.limits.maxPerStageDescriptorSampledImages;
	vk->features.max_per_stage_descriptor_storage_images = pdp.limits.maxPerStageDescriptorStorageImages;


	/*
	 * Queue properties.
	 */

	uint32_t count = 0;
	vk->vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &count, NULL);
	assert(count != 0);
	assert(count > vk->queue_family_index);

	VkQueueFamilyProperties *props = U_TYPED_ARRAY_CALLOC(VkQueueFamilyProperties, count);
	vk->vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &count, props);

	vk->features.timestamp_valid_bits = props[vk->queue_family_index].timestampValidBits;
	free(props);
}

static void
get_external_image_support(struct vk_bundle *vk,
                           bool depth,
                           VkExternalMemoryHandleTypeFlagBits handle_type,
                           bool *out_importable,
                           bool *out_exportable)
{
	// Note that this is a heuristic: just picked two somewhat-random formats to test with here.
	// Before creating an actual swapchain we check the desired format for real.
	// Not using R16G16B16A16_UNORM because 8bpx linear is discouraged, and not using
	// the SRGB version because Android's AHardwareBuffer is weird with SRGB (no internal support)
	VkFormat image_format = depth ? VK_FORMAT_D16_UNORM : VK_FORMAT_R16G16B16A16_UNORM;
	enum xrt_swapchain_usage_bits bits =
	    depth ? (enum xrt_swapchain_usage_bits)(XRT_SWAPCHAIN_USAGE_DEPTH_STENCIL | XRT_SWAPCHAIN_USAGE_SAMPLED)
	          : (enum xrt_swapchain_usage_bits)(XRT_SWAPCHAIN_USAGE_COLOR | XRT_SWAPCHAIN_USAGE_SAMPLED);
	vk_csci_get_image_external_support(vk, image_format, bits, handle_type, out_importable, out_exportable);
}

static bool
is_fence_bit_supported(struct vk_bundle *vk, VkExternalFenceHandleTypeFlagBits handle_type)
{
	VkPhysicalDeviceExternalFenceInfo external_fence_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
	    .handleType = handle_type,
	};
	VkExternalFenceProperties external_fence_props = {
	    .sType = VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES,
	};

	vk->vkGetPhysicalDeviceExternalFencePropertiesKHR( //
	    vk->physical_device,                           // physicalDevice
	    &external_fence_info,                          // pExternalFenceInfo
	    &external_fence_props);                        // pExternalFenceProperties

	const VkExternalFenceFeatureFlagBits bits =    //
	    VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT | //
	    VK_EXTERNAL_FENCE_FEATURE_IMPORTABLE_BIT;  //

	VkExternalFenceFeatureFlagBits masked = bits & external_fence_props.externalFenceFeatures;
	if (masked != bits) {
		// All must be supported.
		return false;
	}

	return true;
}

static void
get_binary_semaphore_bit_support(struct vk_bundle *vk,
                                 VkExternalSemaphoreHandleTypeFlagBits handle_type,
                                 bool *out_importable,
                                 bool *out_exportable)
{
	VkPhysicalDeviceExternalSemaphoreInfo external_semaphore_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
	    .pNext = NULL,
	    .handleType = handle_type,
	};
	VkExternalSemaphoreProperties external_semaphore_props = {
	    .sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
	};

	vk->vkGetPhysicalDeviceExternalSemaphorePropertiesKHR( //
	    vk->physical_device,                               // physicalDevice
	    &external_semaphore_info,                          // pExternalSemaphoreInfo
	    &external_semaphore_props);                        // pExternalSemaphoreProperties

	const VkExternalSemaphoreFeatureFlagBits bits = external_semaphore_props.externalSemaphoreFeatures;

	*out_importable = (bits & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) != 0;
	*out_exportable = (bits & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) != 0;
}

static bool
is_binary_semaphore_bit_supported(struct vk_bundle *vk, VkExternalSemaphoreHandleTypeFlagBits handle_type)
{
	bool importable = false, exportable = false;
	get_binary_semaphore_bit_support(vk, handle_type, &importable, &exportable);

	return importable && exportable;
}

static void
get_timeline_semaphore_bit_support(struct vk_bundle *vk,
                                   VkExternalSemaphoreHandleTypeFlagBits handle_type,
                                   bool *out_importable,
                                   bool *out_exportable)
{
	*out_importable = false;
	*out_exportable = false;

#ifdef VK_KHR_timeline_semaphore
	/*
	 * This technically is for the device not the physical device,
	 * but we can use it as a way to gate running the detection code.
	 */
	if (!vk->features.timeline_semaphore) {
		return;
	}

	VkSemaphoreTypeCreateInfo semaphore_type_create_info = {
	    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
	    .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	    .initialValue = 0,
	};
	VkPhysicalDeviceExternalSemaphoreInfo external_semaphore_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
	    .pNext = (const void *)&semaphore_type_create_info,
	    .handleType = handle_type,
	};
	VkExternalSemaphoreProperties external_semaphore_props = {
	    .sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
	};

	vk->vkGetPhysicalDeviceExternalSemaphorePropertiesKHR( //
	    vk->physical_device,                               // physicalDevice
	    &external_semaphore_info,                          // pExternalSemaphoreInfo
	    &external_semaphore_props);                        // pExternalSemaphoreProperties

	const VkExternalSemaphoreFeatureFlagBits bits = external_semaphore_props.externalSemaphoreFeatures;

	*out_importable = (bits & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) != 0;
	*out_exportable = (bits & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT) != 0;
#endif
}

static bool
is_timeline_semaphore_bit_supported(struct vk_bundle *vk, VkExternalSemaphoreHandleTypeFlagBits handle_type)
{
	bool importable = false, exportable = false;
	get_timeline_semaphore_bit_support(vk, handle_type, &importable, &exportable);

	return importable && exportable;
}

static void
fill_in_external_object_properties(struct vk_bundle *vk)
{
	// Make sure it's cleared.
	U_ZERO(&vk->external);

	if (vk->vkGetPhysicalDeviceExternalFencePropertiesKHR == NULL) {
		VK_WARN(vk, "vkGetPhysicalDeviceExternalFencePropertiesKHR not supported, should always be.");
		return;
	}

	if (vk->vkGetPhysicalDeviceExternalSemaphorePropertiesKHR == NULL) {
		VK_WARN(vk, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR not supported, should always be.");
		return;
	}
	if (vk->vkGetPhysicalDeviceImageFormatProperties2 == NULL) {
		VK_WARN(vk, "vkGetPhysicalDeviceImageFormatProperties2 not supported, should always be.");
		return;
	}

#if defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_WIN32_HANDLE)
	get_external_image_support(vk, false, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
	                           &vk->external.color_image_import_opaque_win32,
	                           &vk->external.color_image_export_opaque_win32);
	get_external_image_support(vk, true, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
	                           &vk->external.depth_image_import_opaque_win32,
	                           &vk->external.depth_image_export_opaque_win32);


	get_external_image_support(vk, false, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
	                           &vk->external.color_image_import_d3d11, &vk->external.color_image_export_d3d11);
	get_external_image_support(vk, true, VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,
	                           &vk->external.depth_image_import_d3d11, &vk->external.depth_image_export_d3d11);

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_FD)
	get_external_image_support(vk, false, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
	                           &vk->external.color_image_import_opaque_fd,
	                           &vk->external.color_image_export_opaque_fd);
	get_external_image_support(vk, true, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
	                           &vk->external.depth_image_import_opaque_fd,
	                           &vk->external.depth_image_export_opaque_fd);

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_AHARDWAREBUFFER)
	get_external_image_support(vk, false, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
	                           &vk->external.color_image_import_opaque_fd,
	                           &vk->external.color_image_export_opaque_fd);
	get_external_image_support(vk, true, VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
	                           &vk->external.depth_image_import_opaque_fd,
	                           &vk->external.depth_image_export_opaque_fd);


	get_external_image_support(vk, false, VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
	                           &vk->external.color_image_import_ahardwarebuffer,
	                           &vk->external.color_image_export_ahardwarebuffer);
	get_external_image_support(vk, true, VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
	                           &vk->external.depth_image_import_ahardwarebuffer,
	                           &vk->external.depth_image_export_ahardwarebuffer);
#endif
#if defined(XRT_GRAPHICS_SYNC_HANDLE_IS_FD)

	vk->external.fence_sync_fd = is_fence_bit_supported( //
	    vk, VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT);
	vk->external.fence_opaque_fd = is_fence_bit_supported( //
	    vk, VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_FD_BIT);

	vk->external.binary_semaphore_sync_fd = is_binary_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
	vk->external.binary_semaphore_opaque_fd = is_binary_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);

	//! @todo Is this safe to assume working, do we need to check an extension?
	vk->external.timeline_semaphore_sync_fd = is_timeline_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT);
	vk->external.timeline_semaphore_opaque_fd = is_timeline_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);

#elif defined(XRT_GRAPHICS_SYNC_HANDLE_IS_WIN32_HANDLE)

	vk->external.fence_win32_handle = is_fence_bit_supported( //
	    vk, VK_EXTERNAL_FENCE_HANDLE_TYPE_OPAQUE_WIN32_BIT);

	vk->external.binary_semaphore_d3d12_fence = is_binary_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT);
	vk->external.binary_semaphore_win32_handle = is_binary_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT);

	//! @todo Is this safe to assume working, do we need to check an extension?
	vk->external.timeline_semaphore_d3d12_fence = is_timeline_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT);
	vk->external.timeline_semaphore_win32_handle = is_timeline_semaphore_bit_supported( //
	    vk, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT);

#else
#error "Need port for fence sync handles checkers"
#endif
}


/*
 *
 * Device creation helper functions.
 *
 */

static int
device_type_priority(VkPhysicalDeviceType device_type)
{
	switch (device_type) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 4;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 2;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: return 1;
	default: return 0;
	}
}

static bool
device_is_preferred(VkPhysicalDeviceProperties *l_device, VkPhysicalDeviceProperties *r_device)
{
	int l_priority = device_type_priority(l_device->deviceType);
	int r_priority = device_type_priority(r_device->deviceType);

	if (l_priority > r_priority)
		return true;

	return false;
}

static void
device_debug_print(struct vk_bundle *vk, const VkPhysicalDeviceProperties *pdp, uint32_t index)
{
	char title[32];
	(void)snprintf(title, sizeof(title), "GPU index %u\n", index);
	vk_print_device_info(vk, U_LOGGING_DEBUG, pdp, index, title);
}

static uint32_t
select_preferred_device(struct vk_bundle *vk, VkPhysicalDevice *devices, uint32_t device_count)
{
	assert(device_count > 0);

	// Default to first if there is only one.
	uint32_t gpu_index = 0;
	VkPhysicalDeviceProperties gpu_properties;
	vk->vkGetPhysicalDeviceProperties(devices[0], &gpu_properties);

	// Loop starts at index 1, so print the first GPU here.
	device_debug_print(vk, &gpu_properties, 0);

	for (uint32_t i = 1; i < device_count; i++) {
		VkPhysicalDeviceProperties pdp;
		vk->vkGetPhysicalDeviceProperties(devices[i], &pdp);

		// Print GPU 1 to device_count here.
		device_debug_print(vk, &pdp, i);

		// Prefer devices based on device type priority, with preference to equal devices with smaller index
		if (device_is_preferred(&pdp, &gpu_properties)) {
			gpu_index = i;
			gpu_properties = pdp;
		}
	}

	return gpu_index;
}

static VkResult
select_physical_device(struct vk_bundle *vk, int forced_index, bool use_device_group)
{
	VkPhysicalDevice *physical_devices = NULL;
	uint32_t gpu_count = 0;
	VkResult ret;

	vk->features.use_device_group = false;

	if (use_device_group) {
		VK_DEBUG(vk, "Vulkan device groups requested, checking for available groups...");
		// Check if a device group exists
		uint32_t device_group_count;
		vk->vkEnumeratePhysicalDeviceGroups(vk->instance, &device_group_count, NULL);

		// Only continue this path if count >= 1 (fallback to single physical device otherwise)
		if (device_group_count >= 1) {
			VkPhysicalDeviceGroupProperties *physical_device_group_properties;
			physical_device_group_properties =
			    U_TYPED_ARRAY_CALLOC(VkPhysicalDeviceGroupProperties, device_group_count);
			for (size_t i = 0; i < device_group_count; ++i) {
				physical_device_group_properties[i].sType =
				    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
				physical_device_group_properties[i].pNext = NULL;
			}

			vk->vkEnumeratePhysicalDeviceGroups(vk->instance, &device_group_count,
			                                    physical_device_group_properties);
			VkPhysicalDeviceGroupProperties selected_physical_group = physical_device_group_properties[0];
			VK_DEBUG(vk, "Device group found with a physical device count of %d.",
			         selected_physical_group.physicalDeviceCount);
			forced_index = 0;
			vk->physical_device = selected_physical_group.physicalDevices[0];
			vk->device_group_properties = selected_physical_group;

			// Print info
			for (size_t i = 0; i < selected_physical_group.physicalDeviceCount; i++) {
				VkPhysicalDeviceProperties pdp;
				vk->vkGetPhysicalDeviceProperties(selected_physical_group.physicalDevices[i], &pdp);

				char title[41];
				(void)snprintf(title, sizeof(title), "Device group physical device number %zu:\n", i);
				vk_print_device_info(vk, U_LOGGING_INFO, &pdp, i, title);
			}

			// Fill out the device memory props as well.
			vk->vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &vk->device_memory_props);
			vk->features.use_device_group = true;
			return VK_SUCCESS;
		} else {
			VK_ERROR(vk,
			         "Device group requested but no group was found, fallback to single physical device.");
		}
	}

	ret = vk_enumerate_physical_devices( //
	    vk,                              // vk_bundle
	    &gpu_count,                      // out_physical_device_count
	    &physical_devices);              // out_physical_devices
	if (ret != VK_SUCCESS) {
		VK_ERROR(vk, "vk_enumerate_physical_devices: %s", vk_result_string(ret));
		return ret;
	}
	if (gpu_count == 0) {
		VK_ERROR(vk, "No physical device found!");
		return VK_ERROR_DEVICE_LOST; // No need to free if zero devices.
	}

	VK_DEBUG(vk, "Choosing Vulkan device index");
	uint32_t gpu_index = 0;
	if (forced_index >= 0) {
		uint32_t uint_index = (uint32_t)forced_index;
		if (uint_index + 1 > gpu_count) {
			VK_ERROR(vk, "Attempted to force GPU index %u, but only %u GPUs are available", uint_index,
			         gpu_count);
			free(physical_devices);
			return VK_ERROR_DEVICE_LOST;
		}
		gpu_index = uint_index;
		VK_DEBUG(vk, "Forced use of Vulkan device index %u.", gpu_index);
	} else {
		VK_DEBUG(vk, "Available GPUs");
		gpu_index = select_preferred_device(vk, physical_devices, gpu_count);
	}

	// Setup the physical device on the bundle.
	vk->physical_device = physical_devices[gpu_index];
	vk->physical_device_index = gpu_index;

	// Free the array.
	free(physical_devices);
	physical_devices = NULL;


	/*
	 * Have now selected device, get properties of it.
	 */

	VkPhysicalDeviceProperties pdp;
	vk->vkGetPhysicalDeviceProperties(vk->physical_device, &pdp);

	char title[20];
	(void)snprintf(title, sizeof(title), "Selected GPU: %u\n", gpu_index);
	vk_print_device_info(vk, U_LOGGING_DEBUG, &pdp, gpu_index, title);

	char *tegra_substr = strstr(pdp.deviceName, "Tegra");
	if (tegra_substr) {
		vk->is_tegra = true;
		VK_DEBUG(vk, "Detected Tegra, using Tegra specific workarounds!");
	}

	// Fill out the device memory props as well.
	vk->vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &vk->device_memory_props);

	return VK_SUCCESS;
}

static VkResult
find_graphics_queue_family(struct vk_bundle *vk, uint32_t *out_graphics_queue_family)
{
	/* Find the first graphics queue */
	uint32_t queue_family_count = 0;
	uint32_t i = 0;
	vk->vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &queue_family_count, NULL);

	VkQueueFamilyProperties *queue_family_props = U_TYPED_ARRAY_CALLOC(VkQueueFamilyProperties, queue_family_count);

	vk->vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &queue_family_count, queue_family_props);

	if (queue_family_count == 0) {
		VK_DEBUG(vk, "Failed to get queue properties");
		goto err_free;
	}

	for (i = 0; i < queue_family_count; i++) {
		if (queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			break;
		}
	}

	if (i >= queue_family_count) {
		VK_DEBUG(vk, "No graphics queue found");
		goto err_free;
	}

	*out_graphics_queue_family = i;

	free(queue_family_props);

	return VK_SUCCESS;

err_free:
	free(queue_family_props);
	return VK_ERROR_INITIALIZATION_FAILED;
}

static VkResult
find_queue_family(struct vk_bundle *vk, VkQueueFlags required_flags, uint32_t *out_queue_family)
{
	/* Find the "best" queue with the requested flags (prefer queues without graphics) */
	uint32_t queue_family_count = 0;
	uint32_t i = 0;
	vk->vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &queue_family_count, NULL);

	VkQueueFamilyProperties *queue_family_props = U_TYPED_ARRAY_CALLOC(VkQueueFamilyProperties, queue_family_count);

	vk->vkGetPhysicalDeviceQueueFamilyProperties(vk->physical_device, &queue_family_count, queue_family_props);

	if (queue_family_count == 0) {
		VK_DEBUG(vk, "Failed to get queue properties");
		goto err_free;
	}

	for (i = 0; i < queue_family_count; i++) {
		if ((queue_family_props[i].queueFlags & required_flags) != required_flags) {
			continue;
		}

		if (~queue_family_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			break;
		}
	}

	if (i >= queue_family_count) {
		/* If there's no suitable queue without graphics, just find any suitabable one*/
		for (i = 0; i < queue_family_count; i++) {
			if ((queue_family_props[i].queueFlags & required_flags) == required_flags) {
				break;
			}
		}

		if (i >= queue_family_count) {
			VK_DEBUG(vk, "No compatible queue family found (flags: 0x%xd)", required_flags);
			goto err_free;
		}
	}

	*out_queue_family = i;

	free(queue_family_props);

	return VK_SUCCESS;

err_free:
	free(queue_family_props);

	return VK_ERROR_INITIALIZATION_FAILED;
}

static bool
check_extension(struct vk_bundle *vk, VkExtensionProperties *props, uint32_t prop_count, const char *ext)
{
	for (uint32_t i = 0; i < prop_count; i++) {
		if (strcmp(props[i].extensionName, ext) == 0) {
			return true;
		}
	}

	return false;
}

static void
fill_in_has_device_extensions(struct vk_bundle *vk, struct u_string_list *ext_list)
{
	// beginning of GENERATED device extension code - do not modify - used by scripts
	// Reset before filling out.
	vk->has_KHR_buffer_device_address = false;
	vk->has_KHR_external_fence_fd = false;
	vk->has_KHR_external_semaphore_fd = false;
	vk->has_KHR_format_feature_flags2 = false;
	vk->has_KHR_global_priority = false;
	vk->has_KHR_image_format_list = false;
	vk->has_KHR_maintenance1 = false;
	vk->has_KHR_maintenance2 = false;
	vk->has_KHR_maintenance3 = false;
	vk->has_KHR_maintenance4 = false;
	vk->has_KHR_synchronization2 = false;
	vk->has_KHR_timeline_semaphore = false;
	vk->has_EXT_calibrated_timestamps = false;
	vk->has_EXT_display_control = false;
	vk->has_EXT_external_memory_dma_buf = false;
	vk->has_EXT_global_priority = false;
	vk->has_EXT_image_drm_format_modifier = false;
	vk->has_EXT_robustness2 = false;
	vk->has_GOOGLE_display_timing = false;

	const char *const *exts = u_string_list_get_data(ext_list);
	uint32_t ext_count = u_string_list_get_size(ext_list);

	for (uint32_t i = 0; i < ext_count; i++) {
		const char *ext = exts[i];

#if defined(VK_KHR_buffer_device_address)
		if (strcmp(ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME) == 0) {
			vk->has_KHR_buffer_device_address = true;
			continue;
		}
#endif // defined(VK_KHR_buffer_device_address)

#if defined(VK_KHR_external_fence_fd)
		if (strcmp(ext, VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME) == 0) {
			vk->has_KHR_external_fence_fd = true;
			continue;
		}
#endif // defined(VK_KHR_external_fence_fd)

#if defined(VK_KHR_external_semaphore_fd)
		if (strcmp(ext, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME) == 0) {
			vk->has_KHR_external_semaphore_fd = true;
			continue;
		}
#endif // defined(VK_KHR_external_semaphore_fd)

#if defined(VK_KHR_format_feature_flags2)
		if (strcmp(ext, VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME) == 0) {
			vk->has_KHR_format_feature_flags2 = true;
			continue;
		}
#endif // defined(VK_KHR_format_feature_flags2)

#if defined(VK_KHR_global_priority)
		if (strcmp(ext, VK_KHR_GLOBAL_PRIORITY_EXTENSION_NAME) == 0) {
			vk->has_KHR_global_priority = true;
			continue;
		}
#endif // defined(VK_KHR_global_priority)

#if defined(VK_KHR_image_format_list)
		if (strcmp(ext, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME) == 0) {
			vk->has_KHR_image_format_list = true;
			continue;
		}
#endif // defined(VK_KHR_image_format_list)

#if defined(VK_KHR_maintenance1)
		if (strcmp(ext, VK_KHR_MAINTENANCE_1_EXTENSION_NAME) == 0) {
			vk->has_KHR_maintenance1 = true;
			continue;
		}
#endif // defined(VK_KHR_maintenance1)

#if defined(VK_KHR_maintenance2)
		if (strcmp(ext, VK_KHR_MAINTENANCE_2_EXTENSION_NAME) == 0) {
			vk->has_KHR_maintenance2 = true;
			continue;
		}
#endif // defined(VK_KHR_maintenance2)

#if defined(VK_KHR_maintenance3)
		if (strcmp(ext, VK_KHR_MAINTENANCE_3_EXTENSION_NAME) == 0) {
			vk->has_KHR_maintenance3 = true;
			continue;
		}
#endif // defined(VK_KHR_maintenance3)

#if defined(VK_KHR_maintenance4)
		if (strcmp(ext, VK_KHR_MAINTENANCE_4_EXTENSION_NAME) == 0) {
			vk->has_KHR_maintenance4 = true;
			continue;
		}
#endif // defined(VK_KHR_maintenance4)

#if defined(VK_KHR_synchronization2)
		if (strcmp(ext, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0) {
			vk->has_KHR_synchronization2 = true;
			continue;
		}
#endif // defined(VK_KHR_synchronization2)

#if defined(VK_KHR_timeline_semaphore)
		if (strcmp(ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) {
			vk->has_KHR_timeline_semaphore = true;
			continue;
		}
#endif // defined(VK_KHR_timeline_semaphore)

#if defined(VK_EXT_calibrated_timestamps)
		if (strcmp(ext, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME) == 0) {
			vk->has_EXT_calibrated_timestamps = true;
			continue;
		}
#endif // defined(VK_EXT_calibrated_timestamps)

#if defined(VK_EXT_display_control)
		if (strcmp(ext, VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME) == 0) {
			vk->has_EXT_display_control = true;
			continue;
		}
#endif // defined(VK_EXT_display_control)

#if defined(VK_EXT_external_memory_dma_buf)
		if (strcmp(ext, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) == 0) {
			vk->has_EXT_external_memory_dma_buf = true;
			continue;
		}
#endif // defined(VK_EXT_external_memory_dma_buf)

#if defined(VK_EXT_global_priority)
		if (strcmp(ext, VK_EXT_GLOBAL_PRIORITY_EXTENSION_NAME) == 0) {
			vk->has_EXT_global_priority = true;
			continue;
		}
#endif // defined(VK_EXT_global_priority)

#if defined(VK_EXT_image_drm_format_modifier)
		if (strcmp(ext, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME) == 0) {
			vk->has_EXT_image_drm_format_modifier = true;
			continue;
		}
#endif // defined(VK_EXT_image_drm_format_modifier)

#if defined(VK_EXT_robustness2)
		if (strcmp(ext, VK_EXT_ROBUSTNESS_2_EXTENSION_NAME) == 0) {
			vk->has_EXT_robustness2 = true;
			continue;
		}
#endif // defined(VK_EXT_robustness2)

#if defined(VK_GOOGLE_display_timing)
		if (strcmp(ext, VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME) == 0) {
			vk->has_GOOGLE_display_timing = true;
			continue;
		}
#endif // defined(VK_GOOGLE_display_timing)
	}
	// end of GENERATED device extension code - do not modify - used by scripts
}

static bool
should_skip_optional_device_ext(struct vk_bundle *vk,
                                struct u_string_list *required_device_ext_list,
                                struct u_string_list *optional_device_ext_listconst,
                                const char *ext)
{
#ifdef VK_EXT_display_control
	// only enable VK_EXT_display_control when we enabled VK_EXT_display_surface_counter instance ext
	if (strcmp(ext, VK_EXT_DISPLAY_CONTROL_EXTENSION_NAME) == 0) {
		if (!vk->has_EXT_display_surface_counter) {
			VK_DEBUG(vk, "Skipping optional instance extension %s because %s instance ext is not enabled",
			         ext, VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME);
			return true;
		}
		VK_DEBUG(vk, "Not skipping optional instance extension %s because %s instance ext is enabled", ext,
		         VK_EXT_DISPLAY_SURFACE_COUNTER_EXTENSION_NAME);
	}
#endif
	return false;
}

static bool
build_device_extensions(struct vk_bundle *vk,
                        VkPhysicalDevice physical_device,
                        struct u_string_list *required_device_ext_list,
                        struct u_string_list *optional_device_ext_list,
                        struct u_string_list **out_device_ext_list)
{
	VkExtensionProperties *props = NULL;
	uint32_t prop_count = 0;
	VkResult ret;

	ret = vk_enumerate_physical_device_extension_properties( //
	    vk,                                                  // vk_bundle
	    physical_device,                                     // physical_device
	    NULL,                                                // layer_name
	    &prop_count,                                         // out_prop_count
	    &props);                                             // out_props
	if (ret != VK_SUCCESS) {
		VK_ERROR(vk, "vk_enumerate_physical_device_extension_properties: %s", vk_result_string(ret));
		return false;
	}

	uint32_t required_device_ext_count = u_string_list_get_size(required_device_ext_list);
	const char *const *required_device_exts = u_string_list_get_data(required_device_ext_list);

	// error out if we don't support one of the required extensions
	for (uint32_t i = 0; i < required_device_ext_count; i++) {
		const char *ext = required_device_exts[i];
		if (!check_extension(vk, props, prop_count, ext)) {
			VK_DEBUG(vk, "VkPhysicalDevice does not support required extension %s", ext);
			free(props);
			return false;
		}
		VK_DEBUG(vk, "Using required device ext %s", ext);
	}


	*out_device_ext_list = u_string_list_create_from_list(required_device_ext_list);


	uint32_t optional_device_ext_count = u_string_list_get_size(optional_device_ext_list);
	const char *const *optional_device_exts = u_string_list_get_data(optional_device_ext_list);

	for (uint32_t i = 0; i < optional_device_ext_count; i++) {
		const char *ext = optional_device_exts[i];

		if (should_skip_optional_device_ext(vk, required_device_ext_list, optional_device_ext_list, ext)) {
			continue;
		}

		if (check_extension(vk, props, prop_count, ext)) {
			VK_DEBUG(vk, "Using optional device ext %s", ext);
			int added = u_string_list_append_unique(*out_device_ext_list, ext);
			if (added == 0) {
				VK_WARN(vk, "Duplicate device extension %s not added twice", ext);
			}
		} else {
			VK_DEBUG(vk, "NOT using optional device ext %s", ext);
			continue;
		}
	}

	// Fill this out here.
	fill_in_has_device_extensions(vk, *out_device_ext_list);

	free(props);


	return true;
}

/*!
 * @brief Sets fields in @p device_features to true if and only if they are available and they are true in @p
 * optional_device_features (indicating a desire for that feature)
 *
 * @param vk self
 * @param physical_device The physical device to query
 * @param[in] optional_device_features The features to request if available
 * @param[out] device_features Populated with the subset of @p optional_device_features that are actually available.
 */
static void
filter_device_features(struct vk_bundle *vk,
                       VkPhysicalDevice physical_device,
                       const struct vk_device_features *optional_device_features,
                       struct vk_device_features *device_features)
{
	// If no features are requested, then noop.
	if (optional_device_features == NULL) {
		return;
	}

	/*
	 * The structs
	 */

#ifdef VK_EXT_robustness2
	VkPhysicalDeviceRobustness2FeaturesEXT robust_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
	    .pNext = NULL,
	};
#endif

#ifdef VK_KHR_buffer_device_address
	VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
	    .pNext = NULL,
	};
#endif

#ifdef VK_KHR_timeline_semaphore
	VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
	    .pNext = NULL,
	};
#endif

#ifdef VK_KHR_synchronization2
	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization_2_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
	    .pNext = NULL,
	};
#endif

	VkPhysicalDeviceFeatures2 physical_device_features = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
	    .pNext = NULL,
	};

#ifdef VK_EXT_robustness2
	if (vk->has_EXT_robustness2) {
		append_to_pnext_chain((VkBaseInStructure *)&physical_device_features,
		                      (VkBaseInStructure *)&robust_info);
	}
#endif

#ifdef VK_KHR_buffer_device_address
	if (vk->has_KHR_buffer_device_address) {
		append_to_pnext_chain((VkBaseInStructure *)&physical_device_features,
		                      (VkBaseInStructure *)&buffer_device_address_info);
	}
#endif

#ifdef VK_KHR_timeline_semaphore
	if (vk->has_KHR_timeline_semaphore) {
		append_to_pnext_chain((VkBaseInStructure *)&physical_device_features,
		                      (VkBaseInStructure *)&timeline_semaphore_info);
	}
#endif

#ifdef VK_KHR_synchronization2
	if (vk->has_KHR_synchronization2) {
		append_to_pnext_chain((VkBaseInStructure *)&physical_device_features,
		                      (VkBaseInStructure *)&synchronization_2_info);
	}
#endif

	vk->vkGetPhysicalDeviceFeatures2( //
	    physical_device,              // physicalDevice
	    &physical_device_features);   // pFeatures


	/*
	 * Collect and transfer.
	 */

#define CHECK(feature, DEV_FEATURE) device_features->feature = optional_device_features->feature && (DEV_FEATURE)

#ifdef VK_EXT_robustness2
	CHECK(null_descriptor, robust_info.nullDescriptor);
#endif

#ifdef VK_KHR_buffer_device_address
	CHECK(buffer_device_address, buffer_device_address_info.bufferDeviceAddress);
#endif

#ifdef VK_KHR_timeline_semaphore
	CHECK(timeline_semaphore, timeline_semaphore_info.timelineSemaphore);
#endif

#ifdef VK_KHR_synchronization2
	CHECK(synchronization_2, synchronization_2_info.synchronization2);
#endif

	CHECK(shader_image_gather_extended, physical_device_features.features.shaderImageGatherExtended);

	CHECK(shader_storage_image_write_without_format,
	      physical_device_features.features.shaderStorageImageWriteWithoutFormat);

#undef CHECK


	VK_DEBUG(vk,
	         "Features:"
	         "\n\tbuffer_device_address: %i"
	         "\n\tnull_descriptor: %i"
	         "\n\tshader_image_gather_extended: %i"
	         "\n\tshader_storage_image_write_without_format: %i"
	         "\n\ttimeline_semaphore: %i"
	         "\n\tsynchronization_2: %i",                                //
	         device_features->buffer_device_address,                     //
	         device_features->null_descriptor,                           //
	         device_features->shader_image_gather_extended,              //
	         device_features->shader_storage_image_write_without_format, //
	         device_features->timeline_semaphore,                        //
	         device_features->synchronization_2);
}


/*
 *
 * 'Exported' device functions.
 *
 */

VkResult
vk_select_physical_device(struct vk_bundle *vk, int forced_index, bool use_device_group)
{
	return select_physical_device(vk, forced_index, use_device_group);
}

XRT_CHECK_RESULT VkResult
vk_create_device(struct vk_bundle *vk,
                 int forced_index,
                 bool only_compute,
                 bool use_device_group,
                 VkQueueGlobalPriorityEXT global_priority,
                 struct u_string_list *required_device_ext_list,
                 struct u_string_list *optional_device_ext_list,
                 const struct vk_device_features *optional_device_features)
{
	VkResult ret;

	ret = select_physical_device(vk, forced_index, use_device_group);
	if (ret != VK_SUCCESS) {
		return ret;
	}

	struct u_string_list *device_ext_list = NULL;
	if (!build_device_extensions(vk, vk->physical_device, required_device_ext_list, optional_device_ext_list,
	                             &device_ext_list)) {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}


	/*
	 * Features
	 */

	struct vk_device_features device_features = {0};
	filter_device_features(vk, vk->physical_device, optional_device_features, &device_features);
	vk->features.timeline_semaphore = device_features.timeline_semaphore;
	vk->features.synchronization_2 = device_features.synchronization_2;
	vk->features.buffer_device_address = device_features.buffer_device_address;

	/*
	 * Queue
	 */

	// If we don't have global priority, only allow medium priority queues.
	if (!vk->has_EXT_global_priority && //
	    !vk->has_KHR_global_priority && //
	    global_priority != VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT) {
		return VK_ERROR_NOT_PERMITTED_EXT;
	}

	if (only_compute) {
		ret = find_queue_family(vk, VK_QUEUE_COMPUTE_BIT, &vk->queue_family_index);
	} else {
		ret = find_graphics_queue_family(vk, &vk->queue_family_index);
	}

	if (ret != VK_SUCCESS) {
		return ret;
	}

	VkDeviceQueueGlobalPriorityCreateInfoEXT priority_info = {
	    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
	    .pNext = NULL,
	    .globalPriority = global_priority,
	};

	float queue_priority = 0.0f;
	VkDeviceQueueCreateInfo queue_create_info[2] = {0};
	uint32_t queue_create_info_count = 1;

	// Compute or Graphics queue
	queue_create_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info[0].pNext = NULL;
	queue_create_info[0].queueCount = 1;
	queue_create_info[0].queueFamilyIndex = vk->queue_family_index;
	queue_create_info[0].pQueuePriorities = &queue_priority;

#ifdef VK_KHR_video_encode_queue
	// Video encode queue
	vk->encode_queue_family_index = VK_QUEUE_FAMILY_IGNORED;
	if (u_string_list_contains(device_ext_list, VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME)) {
		ret = find_queue_family(vk, VK_QUEUE_VIDEO_ENCODE_BIT_KHR, &vk->encode_queue_family_index);
		if (ret == VK_SUCCESS) {
			queue_create_info[queue_create_info_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_create_info[queue_create_info_count].pNext = NULL;
			queue_create_info[queue_create_info_count].queueCount = 1;
			queue_create_info[queue_create_info_count].queueFamilyIndex = vk->encode_queue_family_index;
			queue_create_info[queue_create_info_count].pQueuePriorities = &queue_priority;
			queue_create_info_count++;
			VK_DEBUG(vk, "Creating video encode queue, family index %d", vk->encode_queue_family_index);
		}
	}
#endif

#ifdef VK_KHR_global_priority
	static_assert(VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT ==
	                  VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_KHR,
	              "Vulkan structs doesn't have the same structure ID!");
#endif

	if (vk->has_EXT_global_priority || vk->has_KHR_global_priority) {
		// This is okay, see static_assert above.
		priority_info.pNext = queue_create_info[0].pNext;
		queue_create_info[0].pNext = (void *)&priority_info;
	}


	/*
	 * Device
	 */

#ifdef VK_EXT_robustness2
	VkPhysicalDeviceRobustness2FeaturesEXT robust_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
	    .pNext = NULL,
	    .nullDescriptor = device_features.null_descriptor,
	};
#endif

#ifdef VK_KHR_buffer_device_address
	VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
	    .pNext = NULL,
	    .bufferDeviceAddress = device_features.buffer_device_address,
	    .bufferDeviceAddressCaptureReplay = VK_FALSE,
	    .bufferDeviceAddressMultiDevice = VK_TRUE,
	};
#endif

#ifdef VK_KHR_timeline_semaphore
	VkPhysicalDeviceTimelineSemaphoreFeaturesKHR timeline_semaphore_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES_KHR,
	    .pNext = NULL,
	    .timelineSemaphore = device_features.timeline_semaphore,
	};
#endif

#ifdef VK_KHR_synchronization2
	VkPhysicalDeviceSynchronization2FeaturesKHR synchronization_2_info = {
	    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
	    .pNext = NULL,
	    .synchronization2 = device_features.synchronization_2,
	};
#endif

	VkPhysicalDeviceFeatures enabled_features = {
	    .shaderImageGatherExtended = device_features.shader_image_gather_extended,
	    .shaderStorageImageWriteWithoutFormat = device_features.shader_storage_image_write_without_format,
	};

	VkDeviceCreateInfo device_create_info = {
	    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
	    .queueCreateInfoCount = queue_create_info_count,
	    .pQueueCreateInfos = queue_create_info,
	    .enabledExtensionCount = u_string_list_get_size(device_ext_list),
	    .ppEnabledExtensionNames = u_string_list_get_data(device_ext_list),
	    .pEnabledFeatures = &enabled_features,
	};

#ifdef VK_EXT_robustness2
	if (vk->has_EXT_robustness2) {
		append_to_pnext_chain((VkBaseInStructure *)&device_create_info, (VkBaseInStructure *)&robust_info);
	}
#endif

#ifdef VK_KHR_buffer_device_address
	if (vk->has_KHR_buffer_device_address) {
		append_to_pnext_chain((VkBaseInStructure *)&device_create_info,
		                      (VkBaseInStructure *)&buffer_device_address_info);
	}
#endif

#ifdef VK_KHR_timeline_semaphore
	if (vk->has_KHR_timeline_semaphore) {
		append_to_pnext_chain((VkBaseInStructure *)&device_create_info,
		                      (VkBaseInStructure *)&timeline_semaphore_info);
	}
#endif

#ifdef VK_KHR_synchronization2
	if (vk->has_KHR_synchronization2) {
		append_to_pnext_chain((VkBaseInStructure *)&device_create_info,
		                      (VkBaseInStructure *)&synchronization_2_info);
	}
#endif

	if (vk->features.use_device_group) {
		VkDeviceGroupDeviceCreateInfo device_group_create_info;
		device_group_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO;
		device_group_create_info.pNext = NULL;
		device_group_create_info.physicalDeviceCount = vk->device_group_properties.physicalDeviceCount;
		device_group_create_info.pPhysicalDevices = vk->device_group_properties.physicalDevices;
		append_to_pnext_chain((VkBaseInStructure *)&device_create_info,
		                      (VkBaseInStructure *)&device_group_create_info);
	}

	ret = vk->vkCreateDevice(vk->physical_device, &device_create_info, NULL, &vk->device);

	u_string_list_destroy(&device_ext_list);

	if (ret != VK_SUCCESS) {
		VK_DEBUG(vk, "vkCreateDevice: %s (%d)", vk_result_string(ret), ret);
		if (ret == VK_ERROR_NOT_PERMITTED_EXT) {
			VK_DEBUG(vk, "Is CAP_SYS_NICE set? Try: sudo setcap cap_sys_nice+ep monado-service");
		}
		return ret;
	}

	// Fill in the device features we are interested in.
	fill_in_device_features(vk);

	// We fill in these here as we want to be sure we have selected the physical device fully.
	fill_in_external_object_properties(vk);

	// Now setup all of the device specific functions.
	ret = vk_get_device_functions(vk);
	if (ret != VK_SUCCESS) {
		goto err_destroy;
	}
	vk->vkGetDeviceQueue(vk->device, vk->queue_family_index, 0, &vk->queue);
#if defined(VK_KHR_video_encode_queue)
	if (vk->encode_queue_family_index != VK_QUEUE_FAMILY_IGNORED) {
		vk->vkGetDeviceQueue(vk->device, vk->encode_queue_family_index, 0, &vk->encode_queue);
	}
#endif

	// Need to do this after functions have been gotten.
	VK_NAME_INSTANCE(vk, vk->instance, "vk_bundle instance");
	VK_NAME_DEVICE(vk, vk->device, "vk_bundle device");

	return ret;

err_destroy:
	vk->vkDestroyDevice(vk->device, NULL);
	vk->device = NULL;

	return ret;
}

VkResult
vk_init_mutex(struct vk_bundle *vk)
{
	if (os_mutex_init(&vk->queue_mutex) < 0) {
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	return VK_SUCCESS;
}

VkResult
vk_deinit_mutex(struct vk_bundle *vk)
{
	os_mutex_destroy(&vk->queue_mutex);
	return VK_SUCCESS;
}


/*
 *
 * Complete setup.
 *
 */

XRT_CHECK_RESULT VkResult
vk_init_from_given(struct vk_bundle *vk,
                   PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr,
                   VkInstance instance,
                   VkPhysicalDevice physical_device,
                   VkDevice device,
                   uint32_t queue_family_index,
                   uint32_t queue_index,
                   bool external_fence_fd_enabled,
                   bool external_semaphore_fd_enabled,
                   bool timeline_semaphore_enabled,
                   bool debug_utils_enabled,
                   enum u_logging_level log_level)
{
	VkResult ret;

	// First memset it clear.
	U_ZERO(vk);
	vk->log_level = log_level;

	ret = vk_get_loader_functions(vk, vkGetInstanceProcAddr);
	if (ret != VK_SUCCESS) {
		goto err_memset;
	}

	vk->instance = instance;
	vk->physical_device = physical_device;
	vk->device = device;
	vk->queue_family_index = queue_family_index;
	vk->queue_index = queue_index;

	// Fill in all instance functions.
	ret = vk_get_instance_functions(vk);
	if (ret != VK_SUCCESS) {
		goto err_memset;
	}

	// Fill out the device memory props here, as we are
	// passed a vulkan context and do not call selectPhysicalDevice()
	vk->vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &vk->device_memory_props);

	// Vulkan does not let us read what extensions was enabled.
	if (external_fence_fd_enabled) {
		vk->has_KHR_external_fence_fd = true;
	}

	// Vulkan does not let us read what extensions was enabled.
	if (external_semaphore_fd_enabled) {
		vk->has_KHR_external_semaphore_fd = true;
	}

#ifdef VK_KHR_timeline_semaphore
	/*
	 * Has the timeline semaphore extension and feature been enabled?
	 * Need to do this before fill_in_external_object_properties.
	 */
	if (timeline_semaphore_enabled) {
		vk->has_KHR_timeline_semaphore = true;
		vk->features.timeline_semaphore = true;
	}
#endif

#ifdef VK_EXT_debug_utils
	if (debug_utils_enabled) {
		vk->has_EXT_debug_utils = true;
	}
#endif

	// Fill in the device features we are interested in.
	fill_in_device_features(vk);

	// Fill in external object properties.
	fill_in_external_object_properties(vk);

	// Fill in all device functions.
	ret = vk_get_device_functions(vk);
	if (ret != VK_SUCCESS) {
		goto err_memset;
	}

	vk->vkGetDeviceQueue(vk->device, vk->queue_family_index, vk->queue_index, &vk->queue);


	return VK_SUCCESS;

err_memset:
	U_ZERO(vk);
	return ret;
}

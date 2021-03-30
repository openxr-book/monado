// Copyright 2019-2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Windows 10 direct mode code.
 *
 * Has to implement comp_target instead of comp_target_swapchain because
 * we don't get a VkSurfaceKHR, etc: we manually import images instead.
 *
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup comp_main
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "os/os_threading.h"
#include "xrt/xrt_compiler.h"
#include "main/comp_window.h"
#include "util/u_misc.h"
#include "util/u_pacing.h"
#include "d3d/d3d_dxgi_helpers.hpp"

#include <memory>
#include <algorithm>
#include <exception>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <windows.devices.display.core.interop.h>

#include <d3d11_1.h>
#include <d3d11_4.h>

#include <wil/com.h> // must be after winrt
#include <wil/resource.h>
#include <wil/result_macros.h>

namespace winrtWDDC = winrt::Windows::Devices::Display::Core;
namespace winrtWDD = winrt::Windows::Devices::Display;

/*
 *
 * Private structs.
 *
 */
namespace {

/// Look for a contract that includes a Windows 11 function (TryExecuteTask instead of ExecuteTask)
/// @return
static inline bool
checkForTryExecuteTask()
{
	return winrt::Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(
	    L"Windows.Foundation.UniversalApiContract", 14);
}

class Renderer
{
public:
	explicit Renderer(struct comp_compositor *comp);

	winrt::Windows::Foundation::Collections::IVector<winrtWDDC::DisplayTarget>
	findHmds();

	bool
	hasImages() const noexcept;


private:
	/// The compositor that owns us
	struct comp_compositor *c;

	winrtWDDC::DisplayManager manager;

	/// @name DXGI/D3D11 objects
	/// @{
	wil::com_ptr<IDXGIAdapter> dxgiAdapter;
	wil::com_ptr<ID3D11Device5> d3d11Device;
	wil::com_ptr<ID3D11DeviceContext4> d3d11Context;
	LUID luid;

	wil::com_ptr<ID3D11Fence> d3d11Fence;
	/// @}

	/// @name WinRT objects
	/// @{
	winrtWDDC::DisplayDevice device{nullptr};
	winrtWDDC::DisplayFence fence{nullptr};
	/// @}

};

Renderer::Renderer(struct comp_compositor *comp)
    : c(comp), manager(winrtWDDC::DisplayManager::Create(winrtWDDC::DisplayManagerOptions::EnforceSourceOwnership)),
      dxgiAdapter(xrt::auxiliary::d3d::getAdapterByLUID(c->settings.client_gpu_deviceLUID))

{
	assert(c->settings.client_gpu_deviceLUID_valid);

	// Get some D3D11 stuff mainly for fence handling
	{
		wil::com_ptr<ID3D11Device> our_dev;
		wil::com_ptr<ID3D11DeviceContext> our_context;
		std::tie(our_dev, our_context) = xrt::auxiliary::d3d::d3d11::createDevice(adapter, c->log_level);
		our_dev.query_to(d3d11Device.put());
		our_context.query_to(d3d11Context.put());

		THROW_IF_FAILED(d3d11Device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(d3d11Fence.put())));
	}

	// Get the LUID in Windows format
	{
		DXGI_ADAPTER_DESC desc{};
		THROW_IF_FAILED(dxgiAdapter->GetDesc(&desc));
		luid = desc.AdapterLuid;
	}
}

winrt::Windows::Foundation::Collections::IVector<winrtWDDC::DisplayTarget>
Renderer::findHmds()
{
	using std::begin;
	using std::end;
	auto current_targets = manager.GetCurrentTargets();
	std::vector<winrtWDDC::DisplayTarget> hmds;
	std::copy_if(begin(current_targets), end(current_targets), std::back_inserter(hmds),
	             [&](winrtWDDC::DisplayTarget const &target) {
		             try {
			             if (target == nullptr) {
				             COMP_WARN(c, "Skipping target because it's NULL.");
				             return false;
			             }
			             if (target.UsageKind() != winrtWDD::DisplayMonitorUsageKind::HeadMounted) {
				             COMP_INFO(c, "Skipping target because it's not marked as an HMD.");
				             return false;
			             }
			             winrtWDD::DisplayMonitor monitor = target.TryGetMonitor();
			             if (!monitor) {
				             COMP_WARN(c, "Skipping target because can't get the monitor.");
				             return false;
			             }
			             winrtWDDC::DisplayAdapter adapter = target.Adapter();
			             if (!adapter) {
				             COMP_WARN(c, "Skipping target because can't get the adapter.");
				             return false;
			             }

			             LUID thisLuid = {adapter.Id().LowPart, adapter.Id().HighPart};

			             if (thisLuid != luid) {
				             COMP_INFO(c, "Skipping target because LUID doesn't match.");
				             return false;
			             }

			             return true;
		             } catch (winrt::hresult_error const &e) {
			             COMP_ERROR(ct->c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
			                        winrt::to_string(e.message()).c_str());
			             return false;
		             } catch (std::exception const &e) {
			             COMP_ERROR(ct->c, "Caught exception: %s", e.what());
			             return false;
		             }
	             });
	return winrt::single_threaded_vector<winrtWDDC::DisplayTarget>(std::move(hmds));
}

inline bool
Renderer::hasImages() const noexcept {

}

} // namespace

/*!
 * A Windows direct mode (Windows.Devices.Display.Core) output interface.
 *
 * @implements comp_target
 */
struct comp_target_direct_windows
{
	struct comp_target base;
	//! Compositor frame pacing helper
	struct u_pacing_compositor *upc;

	//! If we should use display timing.
	enum comp_target_display_timing_usage timing_usage;

	//! Also works as a frame index.
	int64_t current_frame_id;

	struct
	{
		VkFormat color_format;
		VkColorSpaceKHR color_space;
	} preferred;

	//! Present mode that the system must support.
	// VkPresentModeKHR present_mode;

	struct
	{
		//! Must only be accessed from main compositor thread.
		bool has_started;

		//! Protected by event_thread lock.
		bool should_wait;

		//! Protected by event_thread lock.
		uint64_t last_vblank_ns;

		//! Thread waiting on vblank_event_fence (first pixel out).
		struct os_thread_helper event_thread;
	} vblank;

	std::unique_ptr<Renderer> direct_renderer;
};

/*
 *
 * Functions.
 *
 */

static inline struct vk_bundle *
get_vk(struct comp_target_direct_windows *ctdw)
{
	return &ctdw->base.c->base.vk;
}

static void
destroy_image_views(struct comp_target_direct_windows *ctdw)
{
	if (ctdw->base.images == NULL) {
		return;
	}

	struct vk_bundle *vk = get_vk(ctdw);

	for (uint32_t i = 0; i < ctdw->base.image_count; i++) {
		if (ctdw->base.images[i].view == VK_NULL_HANDLE) {
			continue;
		}

		vk->vkDestroyImageView(vk->device, ctdw->base.images[i].view, NULL);
		ctdw->base.images[i].view = VK_NULL_HANDLE;
	}

	free(ctdw->base.images);
	ctdw->base.images = NULL;
}

static void
create_image_views(struct comp_target_direct_windows *ctdw)
{
	struct vk_bundle *vk = get_vk(ctdw);

#if 0
	vk->vkGetSwapchainImagesKHR( //
	    vk->device,              // device
	    ctdw->swapchain.handle,  // swapchain
	    &ctdw->base.image_count, // pSwapchainImageCount
	    NULL);                   // pSwapchainImages
	assert(ctdw->base.image_count > 0);
	COMP_DEBUG(ctdw->base.c, "Creating %d image views.", ctdw->base.image_count);

	VkImage *images = U_TYPED_ARRAY_CALLOC(VkImage, ctdw->base.image_count);
	vk->vkGetSwapchainImagesKHR( //
	    vk->device,              // device
	    ctdw->swapchain.handle,  // swapchain
	    &ctdw->base.image_count, // pSwapchainImageCount
	    images);                 // pSwapchainImages

	destroy_image_views(ctdw);

	ctdw->base.images = U_TYPED_ARRAY_CALLOC(struct comp_target_image, ctdw->base.image_count);

	VkImageSubresourceRange subresource_range = {
	    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	    .baseMipLevel = 0,
	    .levelCount = 1,
	    .baseArrayLayer = 0,
	    .layerCount = 1,
	};

	for (uint32_t i = 0; i < ctdw->base.image_count; i++) {
		ctdw->base.images[i].handle = images[i];
		vk_create_view(                  //
		    vk,                          // vk_bundle
		    ctdw->base.images[i].handle, // image
		    VK_IMAGE_VIEW_TYPE_2D,       // type
		    ctdw->surface.format.format, // format
		    subresource_range,           // subresource_range
		    &ctdw->base.images[i].view); // out_view
	}

	free(images);
#endif
}

static void
do_update_timings_vblank_thread(struct comp_target_direct_windows *ctdw)
{
	if (!ctdw->vblank.has_started) {
		return;
	}

	uint64_t last_vblank_ns;

	os_thread_helper_lock(&ctdw->vblank.event_thread);
	last_vblank_ns = ctdw->vblank.last_vblank_ns;
	ctdw->vblank.last_vblank_ns = 0;
	os_thread_helper_unlock(&ctdw->vblank.event_thread);

	if (last_vblank_ns) {
		u_pc_update_vblank_from_display_control(ctdw->upc, last_vblank_ns);
	}
}


/*
 *
 * Member functions.
 *
 */

static void
comp_target_direct_windows_create_images(struct comp_target *ct,
                                         uint32_t preferred_width,
                                         uint32_t preferred_height,
                                         VkFormat color_format,
                                         VkColorSpaceKHR color_space,
                                         VkImageUsageFlags image_usage,
                                         VkPresentModeKHR present_mode)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	struct vk_bundle *vk = get_vk(ctdw);
	VkBool32 supported;
	VkResult ret;

	uint64_t now_ns = os_monotonic_get_ns();
	// Some platforms really don't like the pacing_compositor code.
	bool use_display_timing_if_available = ctdw->timing_usage == COMP_TARGET_USE_DISPLAY_IF_AVAILABLE;
	if (ctdw->upc == NULL && use_display_timing_if_available && vk->has_GOOGLE_display_timing) {
		u_pc_display_timing_create(ct->c->settings.nominal_frame_interval_ns,
		                           &U_PC_DISPLAY_TIMING_CONFIG_DEFAULT, &ctdw->upc);
	} else if (ctdw->upc == NULL) {
		u_pc_fake_create(ct->c->settings.nominal_frame_interval_ns, now_ns, &ctdw->upc);
	}

	// Free old image views.
	destroy_image_views(ctdw);

	VkSwapchainKHR old_swapchain_handle = ctdw->swapchain.handle;

	ctdw->base.image_count = 0;
	ctdw->swapchain.handle = VK_NULL_HANDLE;
	ctdw->present_mode = present_mode;
	ctdw->preferred.color_format = color_format;
	ctdw->preferred.color_space = color_space;

	/// @todo find and open the device/target here.

	// Preliminary check of the environment
	ret = vk->vkGetPhysicalDeviceSurfaceSupportKHR( //
	    vk->physical_device,                        // physicalDevice
	    vk->queue_family_index,                     // queueFamilyIndex
	    ctdw->surface.handle,                       // surface
	    &supported);                                // pSupported
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "vkGetPhysicalDeviceSurfaceSupportKHR: %s", vk_result_string(ret));
	} else if (!supported) {
		COMP_ERROR(ct->c, "vkGetPhysicalDeviceSurfaceSupportKHR: Surface not supported!");
	}

	if (!check_surface_present_mode(ctdw, ctdw->surface.handle, ctdw->present_mode)) {
		// Free old.
		destroy_old(ctdw, old_swapchain_handle);
		return;
	}

	// Find the correct format.
	if (!find_surface_format(ctdw, ctdw->surface.handle, &ctdw->surface.format)) {
		// Free old.
		destroy_old(ctdw, old_swapchain_handle);
		return;
	}

	// Get the caps first.
	VkSurfaceCapabilitiesKHR surface_caps;
	ret = vk->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, ctdw->surface.handle, &surface_caps);
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR: %s", vk_result_string(ret));

		// Free old.
		destroy_old(ctdw, old_swapchain_handle);
		return;
	}

	// Get the extents of the swapchain.
	VkExtent2D extent = select_extent(ctdw, surface_caps, preferred_width, preferred_height);

	if (surface_caps.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
	    surface_caps.currentTransform & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
		COMP_DEBUG(ct->c, "Swapping width and height, since we are going to pre rotate");
		uint32_t w2 = extent.width;
		uint32_t h2 = extent.height;
		extent.width = h2;
		extent.height = w2;
	}

	COMP_DEBUG(ct->c, "swapchain minImageCount %d maxImageCount %d", surface_caps.minImageCount,
	           surface_caps.maxImageCount);

	// Get the image count.
	const uint32_t preferred_at_least_image_count = 3;
	uint32_t image_count = select_image_count(ctdw, surface_caps, preferred_at_least_image_count);


	/*
	 * Do the creation.
	 */

	COMP_DEBUG(ct->c, "Creating compositor swapchain with %d images", image_count);

	// Create the swapchain now.
	VkSwapchainCreateInfoKHR swapchain_info = {
	    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
	    .surface = ctdw->surface.handle,
	    .minImageCount = image_count,
	    .imageFormat = ctdw->surface.format.format,
	    .imageColorSpace = ctdw->surface.format.colorSpace,
	    .imageExtent =
	        {
	            .width = extent.width,
	            .height = extent.height,
	        },
	    .imageArrayLayers = 1,
	    .imageUsage = image_usage,
	    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
	    .queueFamilyIndexCount = 0,
	    .preTransform = surface_caps.currentTransform,
	    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
	    .presentMode = ctdw->present_mode,
	    .clipped = VK_TRUE,
	    .oldSwapchain = old_swapchain_handle,
	};

	ret = vk->vkCreateSwapchainKHR(vk->device, &swapchain_info, NULL, &ctdw->swapchain.handle);

	// Always destroy the old.
	destroy_old(ctdw, old_swapchain_handle);

	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "vkCreateSwapchainKHR: %s", vk_result_string(ret));
		return;
	}


	/*
	 * Set target info.
	 */

	ctdw->base.width = extent.width;
	ctdw->base.height = extent.height;
	ctdw->base.format = ctdw->surface.format.format;
	ctdw->base.surface_transform = surface_caps.currentTransform;

	create_image_views(ctdw);

#ifdef VK_EXT_display_control
	if (!check_surface_counter_caps(ct, vk, ctdw)) {
		COMP_ERROR(ct->c, "Failed to query surface counter capabilities");
	}

	if (vk->has_EXT_display_control && ctdw->display != VK_NULL_HANDLE) {
		if (ctdw->vblank.has_started) {
			// Already running.
		} else if (create_vblank_event_thread(ct)) {
			COMP_INFO(ct->c, "Started vblank event thread!");
		} else {
			COMP_ERROR(ct->c, "Failed to register vblank event");
		}
	} else {
		COMP_INFO(ct->c, "Not using vblank event thread!");
	}
#endif
}

static bool
comp_target_direct_windows_has_images(struct comp_target *ct)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	/// @todo probably incomplete
	return ctdw->direct_renderer != nullptr && ctdw->direct_renderer->device != nullptr;
}

static VkResult
comp_target_direct_windows_acquire_next_image(struct comp_target *ct, VkSemaphore semaphore, uint32_t *out_index)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	struct vk_bundle *vk = get_vk(ctdw);

	if (!comp_target_direct_windows_has_images(ct)) {
		//! @todo what error to return here?
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	TODO basically waitframe
}

static VkResult
comp_target_direct_windows_present(struct comp_target *ct,
                                   VkQueue queue,
                                   uint32_t index,
                                   VkSemaphore semaphore,
                                   uint64_t desired_present_time_ns,
                                   uint64_t present_slop_ns)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	struct vk_bundle *vk = get_vk(ctdw);

	assert(ctdw->current_frame_id >= 0);
	assert(ctdw->current_frame_id <= UINT32_MAX);

	VkPresentTimeGOOGLE times = {
	    .presentID = (uint32_t)ctdw->current_frame_id,
	    .desiredPresentTime = desired_present_time_ns - present_slop_ns,
	};

	VkPresentTimesInfoGOOGLE timings = {
	    .sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE,
	    .swapchainCount = 1,
	    .pTimes = &times,
	};

	VkPresentInfoKHR presentInfo = {
	    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
	    .pNext = vk->has_GOOGLE_display_timing ? &timings : NULL,
	    .waitSemaphoreCount = 1,
	    .pWaitSemaphores = &semaphore,
	    .swapchainCount = 1,
	    .pSwapchains = &ctdw->swapchain.handle,
	    .pImageIndices = &index,
	};

	VkResult ret = vk->vkQueuePresentKHR(queue, &presentInfo);

#ifdef VK_EXT_display_control
	if (ctdw->vblank.has_started) {
		os_thread_helper_lock(&ctdw->vblank.event_thread);
		if (!ctdw->vblank.should_wait) {
			ctdw->vblank.should_wait = true;
			os_thread_helper_signal_locked(&ctdw->vblank.event_thread);
		}
		os_thread_helper_unlock(&ctdw->vblank.event_thread);
	}
#endif

	return ret;
}

static bool
comp_target_direct_windows_check_ready(struct comp_target *ct)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	return ctdw->surface.handle != VK_NULL_HANDLE;
}

static bool
comp_target_direct_windows_init_pre_vulkan(struct comp_target *ct)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	return true;
}

static bool
comp_target_direct_windows_init_post_vulkan(struct comp_target *ct, uint32_t width, uint32_t height)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	winrt::init_apartment();
	try {
		ctdw->direct_renderer = std::make_unique<Renderer>(ct->c);

		auto hmds = ctdw->direct_renderer->findHmds();
		if (hmds.Size() == 0) {
			COMP_ERROR(ct->c, "No displays with headset EDID flag set are available.");
			return false;
		}

		return true;
	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(ct->c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return false;
	} catch (std::exception const &e) {
		COMP_ERROR(ct->c, "Caught exception: %s", e.what());
		return false;
	}
	VkResult ret = VK_SUCCESS;

	///@todo
	// ret = comp_target_direct_windows_create_surface(ctdw, &ctdw->base.surface.handle);
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ct->c, "Failed to create surface '%s'!", vk_result_string(ret));
		return false;
	}

	return true;
}


static void
comp_target_direct_windows_destroy(struct comp_target *ct)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;

	struct vk_bundle *vk = get_vk(ctdw);

	// Thread if it has been started must be stopped first.
	if (ctdw->vblank.has_started) {
		// Destroy also stops the thread.
		os_thread_helper_destroy(&ctdw->vblank.event_thread);
		ctdw->vblank.has_started = false;
	}

	destroy_image_views(ctdw);

	if (ctdw->swapchain.handle != VK_NULL_HANDLE) {
		vk->vkDestroySwapchainKHR(  //
		    vk->device,             // device
		    ctdw->swapchain.handle, // swapchain
		    NULL);                  //
		ctdw->swapchain.handle = VK_NULL_HANDLE;
	}

	if (ctdw->surface.handle != VK_NULL_HANDLE) {
		vk->vkDestroySurfaceKHR(  //
		    vk->instance,         // instance
		    ctdw->surface.handle, // surface
		    NULL);                //
		ctdw->surface.handle = VK_NULL_HANDLE;
	}

	u_pc_destroy(&ctdw->upc);

	delete ctdw;
}

static void
comp_target_direct_windows_update_window_title(struct comp_target *ct, const char *title)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
}

static void
comp_target_direct_windows_flush(struct comp_target *ct)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
}

/*
 *
 * Timing member functions.
 *
 */

static void
comp_target_direct_windows_calc_frame_pacing(struct comp_target *ct,
                                             int64_t *out_frame_id,
                                             uint64_t *out_wake_up_time_ns,
                                             uint64_t *out_desired_present_time_ns,
                                             uint64_t *out_present_slop_ns,
                                             uint64_t *out_predicted_display_time_ns)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;

	int64_t frame_id = -1;
	uint64_t wake_up_time_ns = 0;
	uint64_t desired_present_time_ns = 0;
	uint64_t present_slop_ns = 0;
	uint64_t predicted_display_time_ns = 0;
	uint64_t predicted_display_period_ns = 0;
	uint64_t min_display_period_ns = 0;
	uint64_t now_ns = os_monotonic_get_ns();

	u_pc_predict(ctdw->upc,                    //
	             now_ns,                       //
	             &frame_id,                    //
	             &wake_up_time_ns,             //
	             &desired_present_time_ns,     //
	             &present_slop_ns,             //
	             &predicted_display_time_ns,   //
	             &predicted_display_period_ns, //
	             &min_display_period_ns);      //

	ctdw->current_frame_id = frame_id;

	*out_frame_id = frame_id;
	*out_wake_up_time_ns = wake_up_time_ns;
	*out_desired_present_time_ns = desired_present_time_ns;
	*out_predicted_display_time_ns = predicted_display_time_ns;
	*out_present_slop_ns = present_slop_ns;
}

static void
comp_target_direct_windows_mark_timing_point(struct comp_target *ct,
                                             enum comp_target_timing_point point,
                                             int64_t frame_id,
                                             uint64_t when_ns)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	assert(frame_id == ctdw->current_frame_id);

	switch (point) {
	case COMP_TARGET_TIMING_POINT_WAKE_UP:
		u_pc_mark_point(ctdw->upc, U_TIMING_POINT_WAKE_UP, ctdw->current_frame_id, when_ns);
		break;
	case COMP_TARGET_TIMING_POINT_BEGIN:
		u_pc_mark_point(ctdw->upc, U_TIMING_POINT_BEGIN, ctdw->current_frame_id, when_ns);
		break;
	case COMP_TARGET_TIMING_POINT_SUBMIT:
		u_pc_mark_point(ctdw->upc, U_TIMING_POINT_SUBMIT, ctdw->current_frame_id, when_ns);
		break;
	default: assert(false);
	}
}

static VkResult
comp_target_direct_windows_update_timings(struct comp_target *ct)
{
	COMP_TRACE_MARKER();

	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;

	/// @todo

	// do_update_timings_google_display_timing(ctdw);
	// do_update_timings_vblank_thread(ctdw);

	return VK_SUCCESS;
}
struct comp_target *
comp_target_direct_windows_create(struct comp_compositor *c)
{
	std::unique_ptr<comp_target_direct_windows> ctdw = std::make_unique<comp_target_direct_windows>();

	/// @todo we can actually get some timing
	ctdw->timing_usage = COMP_TARGET_FORCE_FAKE_DISPLAY_TIMING;
	os_thread_helper_init(&ctdw->vblank.event_thread);
	ctdw->base.name = "Windows Direct Mode";
	ctdw->base.acquire = comp_target_direct_windows_acquire_next_image;
	ctdw->base.calc_frame_pacing = comp_target_direct_windows_calc_frame_pacing;
	ctdw->base.check_ready = comp_target_direct_windows_check_ready;
	ctdw->base.create_images = comp_target_direct_windows_create_images;
	ctdw->base.destroy = comp_target_direct_windows_destroy;
	ctdw->base.flush = comp_target_direct_windows_flush;
	ctdw->base.has_images = comp_target_direct_windows_has_images;
	ctdw->base.init_post_vulkan = comp_target_direct_windows_init_post_vulkan;
	ctdw->base.init_pre_vulkan = comp_target_direct_windows_init_pre_vulkan;
	ctdw->base.mark_timing_point = comp_target_direct_windows_mark_timing_point;
	ctdw->base.present = comp_target_direct_windows_present;
	ctdw->base.set_title = comp_target_direct_windows_update_window_title;
	ctdw->base.update_timings = comp_target_direct_windows_update_timings;
	ctdw->base.c = c;

	return &(ctdw.release())->base;
}

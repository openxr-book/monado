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


#include "main/comp_target_direct_windows_swapchain.hpp"
#include "xrt/xrt_defines.h"
#include "os/os_threading.h"
#include "xrt/xrt_compiler.h"
#include "main/comp_window.h"
#include "util/u_misc.h"
#include "util/u_pacing.h"
#include "util/u_win32_com_guard.hpp"
#include "d3d/d3d_dxgi_helpers.hpp"
#include "d3d/d3d_dxgi_formats.h"
#include "d3d/d3d_d3d11_helpers.hpp"
#include "d3d/d3d_winrt_helpers.hpp"
#include "d3d/d3d_convertible_luid.hpp"

#include <memory>
#include <algorithm>
#include <exception>
#include <utility>
#include <set>
#include <optional>
#include <vector>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
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
namespace Collections = winrt::Windows::Foundation::Collections;

using xrt::auxiliary::d3d::ConvertibleLuid;
using namespace xrt::compositor::main;
/*
 *
 * Private structs.
 *
 */
namespace {



#define MAKE_STRINGIFY_CASE(WDDC_ENUM)                                                                                 \
	case winrtWDDC::WDDC_ENUM: return #WDDC_ENUM

XRT_CHECK_RESULT constexpr const char *
to_string(winrtWDDC::DisplayManagerResult e)
{
	switch (e) {
		MAKE_STRINGIFY_CASE(DisplayManagerResult::Success);
		MAKE_STRINGIFY_CASE(DisplayManagerResult::UnknownFailure);
		MAKE_STRINGIFY_CASE(DisplayManagerResult::TargetAccessDenied);
		MAKE_STRINGIFY_CASE(DisplayManagerResult::TargetStale);
		MAKE_STRINGIFY_CASE(DisplayManagerResult::RemoteSessionNotSupported);
	}
	return "DisplayManagerResult::UNKNOWN";
}

XRT_CHECK_RESULT constexpr const char *
to_string(winrtWDDC::DisplayStateOperationStatus e)
{
	switch (e) {
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::Success);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::PartialFailure);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::UnknownFailure);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::TargetOwnershipLost);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::SystemStateChanged);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::TooManyPathsForAdapter);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::ModesNotSupported);
		MAKE_STRINGIFY_CASE(DisplayStateOperationStatus::RemoteSessionNotSupported);
	}
	return "DisplayStateOperationStatus::UNKNOWN";
}

XRT_CHECK_RESULT constexpr const char *
to_string(winrtWDDC::DisplayPathStatus e)
{
	switch (e) {
		MAKE_STRINGIFY_CASE(DisplayPathStatus::Unknown);
		MAKE_STRINGIFY_CASE(DisplayPathStatus::Succeeded);
		MAKE_STRINGIFY_CASE(DisplayPathStatus::Pending);
		MAKE_STRINGIFY_CASE(DisplayPathStatus::Failed);
		MAKE_STRINGIFY_CASE(DisplayPathStatus::FailedAsync);
		MAKE_STRINGIFY_CASE(DisplayPathStatus::InvalidatedAsync);
	}
	return "DisplayPathStatus::UNKNOWN";
}



class CompTargetData
{
public:
	explicit CompTargetData(struct comp_compositor *comp);

	bool
	populateAdapterSpecificData();

	bool
	findHmds();

	bool
	openHmd(const winrtWDDC::DisplayTarget &target, DisplayObjects &outObjects) noexcept;

	/// The compositor that owns us
	struct comp_compositor *c;
	xrt::auxiliary::util::ComGuard com_guard;

	/// Whether we can/should use the Windows 11+-only APIs
	xrt::auxiliary::d3d::winrt::SystemApiCapability capabilities;

	winrtWDDC::DisplayManager manager;

	/// The list of possible HMDs available.
	std::vector<winrtWDDC::DisplayTarget> hmds;

	std::set<ConvertibleLuid> luidsWithHmds;

	/// @name DXGI/D3D11 objects
	/// @{
	wil::com_ptr<IDXGIAdapter> dxgiAdapter;
	wil::com_ptr<ID3D11Device5> d3d11Device;
	wil::com_ptr<ID3D11DeviceContext4> d3d11Context;
	std::optional<ConvertibleLuid> luid;

	wil::com_ptr<ID3D11Fence> d3d11RenderCompleteFence;
	/// @}

	wil::unique_handle renderCompleteFenceHandle;

	/// @name WinRT objects that depend on the adapter
	/// @{
	winrtWDDC::DisplayAdapter displayAdapter{nullptr};
	winrtWDDC::DisplayDevice displayDevice{nullptr};

	winrtWDDC::DisplayFence renderCompleteFence{nullptr};

	DisplayObjects objects{nullptr, nullptr, nullptr, nullptr};
	/// @}
};


CompTargetData::CompTargetData(struct comp_compositor *comp)
    : c(comp), manager(winrtWDDC::DisplayManager::Create(winrtWDDC::DisplayManagerOptions::EnforceSourceOwnership))
{
	capabilities.populate();
}

bool
CompTargetData::populateAdapterSpecificData()
{
	try {
		assert(c->settings.client_gpu_deviceLUID_valid);

		dxgiAdapter = xrt::auxiliary::d3d::getAdapterByLUID(c->settings.client_gpu_deviceLUID);
		ConvertibleLuid adapterLuid{c->settings.client_gpu_deviceLUID};
		// Get some D3D11 stuff mainly for fence handling
		{
			wil::com_ptr<ID3D11Device> our_dev;
			wil::com_ptr<ID3D11DeviceContext> our_context;
			std::tie(our_dev, our_context) =
			    xrt::auxiliary::d3d::d3d11::createDevice(dxgiAdapter, c->settings.log_level);
			our_dev.query_to(d3d11Device.put());
			our_context.query_to(d3d11Context.put());

			THROW_IF_FAILED(d3d11Device->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
			                                         IID_PPV_ARGS(d3d11RenderCompleteFence.put())));
		}

		// Get the LUID in Windows format
		// {
		// 	DXGI_ADAPTER_DESC desc{};
		// 	THROW_IF_FAILED(dxgiAdapter->GetDesc(&desc));
		// 	luid = ConvertibleLuid{desc.AdapterLuid};
		// }

		// get the adapter and device for winrt
		{
			winrt::Windows::Graphics::DisplayAdapterId id = adapterLuid;
			displayAdapter = winrtWDDC::DisplayAdapter::FromId(id);
			if (displayAdapter == nullptr) {
				throw std::runtime_error("Could not get adapter by ID in WinRT!");
			}
			// onecore\windows\directx\database\helperlibrary\lib\directxdatabasehelper.cpp(652)\dxgi.dll!00007FF8D2FACF61:
			// (caller: 00007FF8D2FACA68) ReturnHr(28) tid(4e54) 80004002 No such interface supported
			displayDevice = manager.CreateDisplayDevice(displayAdapter);
		}
		// Get the handle for the fence
		{
			wil::unique_handle fenceHandle;
			THROW_IF_FAILED(d3d11RenderCompleteFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
			                                                             fenceHandle.put()));
			renderCompleteFenceHandle = std::move(fenceHandle);
		}
		// get the winrt object for the fence
		{
			winrt::com_ptr<::IInspectable> fence;
			THROW_IF_FAILED(displayDevice.as<IDisplayDeviceInterop>()->OpenSharedHandle(
			    renderCompleteFenceHandle.get(), IID_PPV_ARGS(fence.put())));
			renderCompleteFence = fence.as<winrtWDDC::DisplayFence>();
		}
		return true;
	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return false;
	} catch (std::exception const &e) {
		COMP_ERROR(c, "Caught exception: %s", e.what());
		return false;
	} catch (...) {
		COMP_ERROR(c, "Caught exception");
		return false;
	}
}


struct HmdSearchResults
{
	std::vector<winrtWDDC::DisplayTarget> hmds;
	std::set<ConvertibleLuid> luidsWithHmds;
};

static bool
targetPredicate(struct comp_compositor *c,
                winrtWDDC::DisplayTarget const &target,
                std::optional<ConvertibleLuid> requiredLuid)
{
	try {
		if (target == nullptr) {
			COMP_WARN(c, "Skipping target because it's NULL.");
			return false;
		}
		if (!target.IsConnected()) {
			return false;
		}
		winrtWDD::DisplayMonitor const monitor = target.TryGetMonitor();
		if (!monitor) {
			COMP_WARN(c, "Skipping target because can't get the monitor.");
			return false;
		}
		winrtWDDC::DisplayAdapter const adapter = target.Adapter();
		if (!adapter) {
			COMP_WARN(c, "Skipping target because can't get the adapter.");
			return false;
		}
		auto displayName = winrt::to_string(monitor.DisplayName());
		COMP_INFO(c, "Considering display '%s' on adapter %s", displayName.c_str(),
		          winrt::to_string(adapter.DeviceInterfacePath()).c_str());

		if (target.UsageKind() != winrtWDD::DisplayMonitorUsageKind::HeadMounted) {
			COMP_INFO(c, "Skipping target because it's not marked as an HMD.");
			return false;
		}
		if (requiredLuid) {
			auto thisLuid = ConvertibleLuid{adapter.Id()};
			if (*requiredLuid != thisLuid) {
				COMP_INFO(c, "Skipping target because LUID doesn't match.");
				return false;
			}
		}

		COMP_INFO(c, "Display '%s' meets our requirements for direct mode on Windows!", displayName.c_str());
		return true;
	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return false;
	} catch (std::exception const &e) {
		COMP_ERROR(c, "Caught exception: %s", e.what());
		return false;
	} catch (...) {
		COMP_ERROR(c, "Caught exception");
		return false;
	}
}
HmdSearchResults
findHmds(struct comp_compositor *c,
         const winrtWDDC::DisplayManager &manager,
         std::optional<ConvertibleLuid> requiredLuid)
{
	using std::begin;
	using std::end;
	HmdSearchResults ret;

	auto currentTargets = manager.GetCurrentTargets();

	COMP_INFO(c, "About to filter targets: starting with %d", int(currentTargets.Size()));
	std::copy_if(begin(currentTargets), end(currentTargets), std::back_inserter(ret.hmds),
	             [&](winrtWDDC::DisplayTarget const &target) { return targetPredicate(c, target, requiredLuid); });
	COMP_INFO(c, "Filtering left us with %d possible HMD targets", int(ret.hmds.size()));
	if (!ret.hmds.empty()) {
		for (const auto &target : ret.hmds) {
			ret.luidsWithHmds.insert(ConvertibleLuid(target.Adapter().Id()));
		}
		COMP_INFO(c, "They are on a total of with %d different adapters", int(ret.luidsWithHmds.size()));
	}
	return ret;
}

bool
CompTargetData::findHmds()
{
	HmdSearchResults results = ::findHmds(c, manager, ConvertibleLuid{c->settings.client_gpu_deviceLUID});
	hmds = results.hmds;
	luidsWithHmds = results.luidsWithHmds;
	return !hmds.empty();
}

inline bool
CompTargetData::openHmd(const winrtWDDC::DisplayTarget &target, DisplayObjects &outObjects) noexcept
{
	try {
		if (!target) {
			COMP_WARN(c, "Target is null.");
			return false;
		}
		if (!target.IsConnected()) {
			COMP_WARN(c, "Target is not connected.");
			return false;
		}
		winrtWDD::DisplayMonitor const monitor = target.TryGetMonitor();
		if (!monitor) {
			COMP_WARN(c, "Could not get the monitor.");
			return false;
		}
		COMP_INFO(c, "Will try to open display '%s' on adapter %s",
		          winrt::to_string(monitor.DisplayName()).c_str(),
		          winrt::to_string(displayAdapter.DeviceInterfacePath()).c_str());


		Collections::IVector<winrtWDDC::DisplayTarget> const singleTargetVector =
		    winrt::single_threaded_vector<winrtWDDC::DisplayTarget>({target});

		auto stateResult = manager.TryAcquireTargetsAndCreateEmptyState(singleTargetVector);
		if (!SUCCEEDED(stateResult.ExtendedErrorCode())) {
			COMP_ERROR(c, "Could not acquire target and create empty state.");
			return false;
		}
		winrtWDDC::DisplayState const state = stateResult.State();

		{
			// this path object is just temporary, we can get it back later if everything worked
			// right
			winrtWDDC::DisplayPath const path = state.ConnectTarget(target);
			// Parameters we know
			path.IsInterlaced(false);
			path.Scaling(winrtWDDC::DisplayPathScaling::Identity);

			winrt::Windows::Foundation::Collections::IVectorView<winrtWDDC::DisplayModeInfo> modes =
			    path.FindModes(winrtWDDC::DisplayModeQueryOptions::OnlyPreferredResolution);
			auto it =
			    std::min_element(begin(modes), end(modes), xrt::auxiliary::d3d::winrt::modeComparison);
			winrtWDDC::DisplayModeInfo const mode = *it;
			if (mode == nullptr) {
				COMP_WARN(c, "Could not find a suitable mode.");
				return false;
			}
			path.ApplyPropertiesFromMode(mode);
		}

		// Atomically apply the state
		winrtWDDC::DisplayStateOperationResult const applyResult =
		    state.TryApply(winrtWDDC::DisplayStateApplyOptions::None);
		if (applyResult.Status() != winrtWDDC::DisplayStateOperationStatus::Success) {

			COMP_WARN(c, "Could not apply state: %s", to_string(applyResult.Status()));
			return false;
		}

		// Now, get the full state post-apply
		auto finalStateResult = manager.TryAcquireTargetsAndReadCurrentState(singleTargetVector);
		if (finalStateResult.ErrorCode() != winrtWDDC::DisplayManagerResult::Success) {

			COMP_WARN(c, "Could not acquire and read state: %s", to_string(finalStateResult.ErrorCode()));
			return false;
		}
		// displayState = finalStateResult.State();
		// displayTarget = target;
		// displayDevice = manager.CreateDisplayDevice(target.Adapter());
		// displayPath = displayState.GetPathForTarget(target);
		// std::get<winrtWDDC::DisplayState>(outObjects) = finalStateResult.State();
		// std::get<winrtWDDC::DisplayTarget>(outObjects) = target;
		// std::get<winrtWDDC::DisplayPath>(outObjects) = displayState.GetPathForTarget(target);
		winrtWDDC::DisplayPath displayPath = finalStateResult.State().GetPathForTarget(target);

		/// @todo This throws an exception right now:
		// onecoreuap\windows\directx\dxg\ddisplay\core\ddisplaysource.cpp(210)\DDisplay.dll!00007FF8753A3DF5:
		// (caller: 00007FF87538738B) ReturnHr(11) tid(e4d0) 80070057 The parameter is incorrect.
		//     Msg:[onecoreuap\windows\directx\dxg\ddisplay\core\ddisplaysource.cpp(208)\DDisplay.dll!00007FF87538FA5E:
		//     (caller: 00007FF87538738B) Exception(8) tid(e4d0) 80070057 The parameter is incorrect.
		//     CallContext:[\DeviceCreateScanoutSource\DeviceCreateScanoutSource]
		// ] CallContext:[\DeviceCreateScanoutSource\DeviceCreateScanoutSource]
		// onecoreuap\windows\directx\dxg\ddisplay\core\ddisplaydevice.cpp(113)\DDisplay.dll!00007FF8753873ED:
		// (caller: 00007FF84ABB422F) ReturnHr(12) tid(e4d0) 80070057 The parameter is incorrect.
		//     CallContext:[\DeviceCreateScanoutSource]
		winrtWDDC::DisplaySource displaySource = displayDevice.CreateScanoutSource(target);
		outObjects = std::make_tuple(displayDevice, target, std::move(displayPath), std::move(displaySource));
		return true;
	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return false;
	} catch (std::exception const &e) {
		COMP_ERROR(c, "Caught exception: %s", e.what());
		return false;
	}
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

	std::unique_ptr<CompTargetData> data;
	std::unique_ptr<CompositorSwapchain> swapchain;

	struct vk_image_collection image_collection;
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


	assert(ctdw->base.image_count > 0);
	COMP_DEBUG(ctdw->base.c, "Creating %d image views.", ctdw->base.image_count);
	VkImage *images = U_TYPED_ARRAY_CALLOC(VkImage, ctdw->base.image_count);
	const uint32_t image_count = ctdw->base.image_count;
	std::vector<xrt_image_native> xins(image_count);
	auto interop = ctdw->data->displayDevice.as<::IDisplayDeviceInterop>();
	for (uint32_t i = 0; i < image_count; ++i) {
		xins[i].handle = ctdw->swapchain->getSurfaceHandle(i);
		xins[i].size = 0;                         /// @todo might be wrong
		xins[i].use_dedicated_allocation = false; /// @todo might be wrong
	}
	xrt_swapchain_create_info info{};
	info.create = (enum xrt_swapchain_create_flags)0;
	// need unordered (aka storage) for access from compute shader
	info.bits = (xrt_swapchain_usage_bits)(XRT_SWAPCHAIN_USAGE_COLOR | XRT_SWAPCHAIN_USAGE_UNORDERED_ACCESS);
	info.format = ctdw->base.format;
	info.sample_count = 1;
	info.width = ctdw->base.width;
	info.height = ctdw->base.height;
	info.face_count = 1;
	info.array_size = 1;
	info.mip_count = 1;

	VkResult const ret = vk_ic_from_natives(vk, &info, xins.data(), image_count, &ctdw->image_collection);
	if (ret != VK_SUCCESS) {
		COMP_ERROR(ctdw->base.c, "Could not import display primaries as Vulkan images: %s",
		           vk_result_string(ret));
		return;
	}

	destroy_image_views(ctdw);

	ctdw->base.images = U_TYPED_ARRAY_CALLOC(struct comp_target_image, ctdw->base.image_count);

	VkImageSubresourceRange const subresource_range = {
	    /*.aspectMask = */ VK_IMAGE_ASPECT_COLOR_BIT,
	    /*.baseMipLevel = */ 0,
	    /*.levelCount = */ 1,
	    /*.baseArrayLayer = */ 0,
	    /*.layerCount = */ 1,
	};

	for (uint32_t i = 0; i < image_count; i++) {
		ctdw->base.images[i].handle = ctdw->image_collection.images[i].handle;
		vk_create_view(                  //
		    vk,                          // vk_bundle
		    ctdw->base.images[i].handle, // image
		    VK_IMAGE_VIEW_TYPE_2D,       // type
		    ctdw->base.format,           // format
		    subresource_range,           // subresource_range
		    &ctdw->base.images[i].view); // out_view
	}

	free(images);
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
static bool
try_open_hmds(struct comp_target_direct_windows *ctdw) noexcept
{
	struct comp_target *ct = &ctdw->base;
	const auto numHmds = ctdw->data->hmds.size();

	// Sometimes it takes a few tries.
	for (int attempt = 0; attempt < 3; ++attempt) {
		for (size_t i = 0; i < numHmds; ++i) {

			COMP_INFO(ct->c, "Attempting to open HMD %d, attempt %d", i, attempt);
			if (!ctdw->data->openHmd(ctdw->data->hmds[i], ctdw->data->objects)) {
				COMP_ERROR(ct->c, "Attempt failed.");

			} else {
				COMP_INFO(ct->c, "Successfully opened HMD %d on attempt %d", i, attempt);
				return true;
			}
		}
	}
	COMP_ERROR(ct->c, "Could not open any HMD despite trying repeatedly.");
	return false;
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

	uint64_t const now_ns = os_monotonic_get_ns();
	// Some platforms really don't like the pacing_compositor code.
	bool const use_display_timing_if_available = ctdw->timing_usage == COMP_TARGET_USE_DISPLAY_IF_AVAILABLE;
	if (ctdw->upc == NULL && use_display_timing_if_available && vk->has_GOOGLE_display_timing) {
		u_pc_display_timing_create(ct->c->settings.nominal_frame_interval_ns,
		                           &U_PC_DISPLAY_TIMING_CONFIG_DEFAULT, &ctdw->upc);
	} else if (ctdw->upc == NULL) {
		u_pc_fake_create(ct->c->settings.nominal_frame_interval_ns, now_ns, &ctdw->upc);
	}

	// Free old image views.
	destroy_image_views(ctdw);
	ctdw->swapchain = {};

	ctdw->base.image_count = 0;
	ctdw->preferred.color_format = color_format;
	ctdw->preferred.color_space = color_space;

	winrt::Windows::Graphics::DirectX::DirectXColorSpace dxColorSpace;
	if (!xrt::auxiliary::d3d::winrt::colorSpaceFromVulkan(color_space, dxColorSpace)) {
		COMP_ERROR(ct->c, "Could not get equivalent of Vulkan color space %d", color_space);
		return;
	}

	// Get the image count.
	const uint32_t preferred_at_least_image_count = 3;
	uint32_t const image_count =
	    preferred_at_least_image_count; // select_image_count(ctdw, surface_caps, preferred_at_least_image_count);
	ctdw->base.image_count = image_count;

	/*
	 * Do the creation.
	 */

	COMP_INFO(ct->c, "Creating compositor swapchain with %d images", image_count);
	ctdw->swapchain = std::make_unique<CompositorSwapchain>(ctdw->base.c, std::move(ctdw->data->objects),
	                                                        dxColorSpace, image_count);


	/*
	 * Set target info.
	 */

	ctdw->base.width = ctdw->swapchain->getWidth();
	ctdw->base.height = ctdw->swapchain->getHeight();
	ctdw->base.format = (VkFormat)d3d_dxgi_format_to_vk(ctdw->swapchain->getFormat());
	ctdw->base.surface_transform = (VkSurfaceTransformFlagBitsKHR)0;

	create_image_views(ctdw);

#if 0
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
	return ctdw->data != nullptr && ctdw->swapchain != nullptr;
}

static VkResult
comp_target_direct_windows_acquire_next_image(struct comp_target *ct, uint32_t *out_index)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;


	if (!comp_target_direct_windows_has_images(ct)) {
		//! @todo what error to return here?
		return VK_ERROR_INITIALIZATION_FAILED;
	}
	try {
		*out_index = ctdw->swapchain->acquireNext();
		return VK_SUCCESS;
	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(ctdw->base.c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return VK_ERROR_DEVICE_LOST;
	} catch (std::exception const &e) {
		COMP_ERROR(ctdw->base.c, "Caught exception: %s", e.what());
		return VK_ERROR_DEVICE_LOST;
	}
}

static VkResult
comp_target_direct_windows_present(struct comp_target *ct,
                                   VkQueue queue,
                                   uint32_t index,
                                   uint64_t timeline_semaphore_value,
                                   uint64_t desired_present_time_ns,
                                   uint64_t present_slop_ns)
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	struct vk_bundle *vk = get_vk(ctdw);

	assert(ctdw->current_frame_id >= 0);
	assert(ctdw->current_frame_id <= UINT32_MAX);

	if (!ctdw->swapchain->present(index, ctdw->data->renderCompleteFence, timeline_semaphore_value)) {
		return VK_ERROR_DEVICE_LOST;
	}
#if 0
	if (ctdw->vblank.has_started) {
		os_thread_helper_lock(&ctdw->vblank.event_thread);
		if (!ctdw->vblank.should_wait) {
			ctdw->vblank.should_wait = true;
			os_thread_helper_signal_locked(&ctdw->vblank.event_thread);
		}
		os_thread_helper_unlock(&ctdw->vblank.event_thread);
	}
#endif

	return VK_SUCCESS;
}

static bool
comp_target_direct_windows_check_ready(struct comp_target *ct) noexcept
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	return ctdw->data && !ctdw->data->hmds.empty();
}

static bool
comp_target_direct_windows_init_pre_vulkan(struct comp_target *ct) noexcept
{
	// struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	return true;
}

static bool
comp_target_direct_windows_init_post_vulkan(struct comp_target *ct, uint32_t width, uint32_t height) noexcept
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
	try {
		if (!ctdw->data->populateAdapterSpecificData()) {
			return false;
		}

		struct vk_bundle *vk = get_vk(ctdw);

		ctdw->base.semaphores.render_complete_is_timeline = true;
		VkResult const vkresult = vk_create_semaphore_from_native(
		    vk, ctdw->data->renderCompleteFenceHandle.get(), &ctdw->base.semaphores.render_complete);
		if (vkresult != VK_SUCCESS) {
			COMP_ERROR(ct->c, "Could not import timeline semaphore.");
			return false;
		}

		return try_open_hmds(ctdw);
	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(ct->c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return false;
	} catch (std::exception const &e) {
		COMP_ERROR(ct->c, "Caught exception: %s", e.what());
		return false;
	} catch (...) {
		COMP_ERROR(ct->c, "Caught exception");
		return false;
	}
}


static void
comp_target_direct_windows_destroy(struct comp_target *ct) noexcept
{
	struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;

	// struct vk_bundle *vk = get_vk(ctdw);

	// Thread if it has been started must be stopped first.
	if (ctdw->vblank.has_started) {
		// Destroy also stops the thread.
		os_thread_helper_destroy(&ctdw->vblank.event_thread);
		ctdw->vblank.has_started = false;
	}

	destroy_image_views(ctdw);
	ctdw->swapchain = {};
	ctdw->data = {};

	u_pc_destroy(&ctdw->upc);

	delete ctdw;
}

static void
comp_target_direct_windows_update_window_title(struct comp_target *ct, const char *title) noexcept
{
	// struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
}

static void
comp_target_direct_windows_flush(struct comp_target *ct) noexcept
{
	// struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;
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
	uint64_t const now_ns = os_monotonic_get_ns();

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
                                             uint64_t when_ns) noexcept
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
comp_target_direct_windows_update_timings(struct comp_target *ct) noexcept
{
	COMP_TRACE_MARKER();

	// struct comp_target_direct_windows *ctdw = (struct comp_target_direct_windows *)ct;

	/// @todo

	// do_update_timings_google_display_timing(ctdw);
	// do_update_timings_vblank_thread(ctdw);

	return VK_SUCCESS;
}
struct comp_target *
comp_target_direct_windows_create(struct comp_compositor *c)
{
	try {
		xrt::auxiliary::d3d::winrt::SystemApiCapability capability;
		capability.populate();

		if (!capability.supportsBasicDirectMode) {
			// Cannot use this API on this Windows version
			COMP_INFO(
			    c, "Cannot use Windows direct mode on this OS version.");
			return nullptr;
		}

		std::unique_ptr<comp_target_direct_windows> ctdw = std::make_unique<comp_target_direct_windows>();

		ctdw->data = std::make_unique<CompTargetData>(c);


		if (!ctdw->data->findHmds()) {
			COMP_INFO(
			    c, "No displays with headset EDID flag set are available: cannot use Windows direct mode.");
			return nullptr;
		}

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

	} catch (winrt::hresult_error const &e) {
		COMP_ERROR(c, "Caught WinRT exception: (%" PRId32 ") %s", e.code().value,
		           winrt::to_string(e.message()).c_str());
		return nullptr;
	} catch (std::exception const &e) {
		COMP_ERROR(c, "Caught exception: %s", e.what());
		return nullptr;
	} catch (...) {
		COMP_ERROR(c, "Caught exception");
		return nullptr;
	}
}

// No special detection
static bool
detect(const struct comp_target_factory *ctf, struct comp_compositor *c)
{
	return false;
}

static bool
create_target(const struct comp_target_factory *ctf, struct comp_compositor *c, struct comp_target **out_ct)
{
	struct comp_target *ct = comp_target_direct_windows_create(c);
	if (ct == NULL) {
		return false;
	}

	*out_ct = ct;

	return true;
}

// static const char *instance_extensions[] = {
//     VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
// };

const struct comp_target_factory comp_target_factory_directwinrt = {
    .name = "Microsoft Windows(TM) Direct Mode via WinRT",
    .identifier = "directwinrt",
    .requires_vulkan_for_create = false,
    .is_deferred = true,
    .required_instance_extensions = nullptr,
    .required_instance_extension_count = 0,
    .detect = detect,
    .create_target = create_target,
};

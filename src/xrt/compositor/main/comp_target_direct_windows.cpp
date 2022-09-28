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

#ifndef _SILENCE_CLANG_COROUTINE_MESSAGE
#define _SILENCE_CLANG_COROUTINE_MESSAGE
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
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

#include <memory>
#include <algorithm>
#include <exception>
#include <utility>

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
#include <winrt/impl/Windows.Devices.Display.Core.0.h>
#include <winrt/impl/Windows.Devices.Display.Core.2.h>

namespace winrtWDDC = winrt::Windows::Devices::Display::Core;
namespace winrtWDD = winrt::Windows::Devices::Display;
namespace Collections = winrt::Windows::Foundation::Collections;
using winrt::Windows::Graphics::DirectX::DirectXPixelFormat;
/*
 *
 * Private structs.
 *
 */
namespace {

/// We retry opening an HMD a few times since it sometimes fails spuriously
constexpr int kMaxOpenAttempts = 2;

inline bool
checkForBasicAPI()
{
	return winrt::Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(
	    L"Windows.Foundation.UniversalApiContract", 7);
}

/// Look for a contract that includes a Windows 11 function (TryExecuteTask instead of ExecuteTask)
/// @return
inline bool
checkForApi14()
{
	return winrt::Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(
	    L"Windows.Foundation.UniversalApiContract", 14);
}

constexpr const char *
to_string(winrtWDDC::DisplayManagerResult e)
{
	switch (e) {
	case winrtWDDC::DisplayManagerResult::Success: return "DisplayManagerResult::Success";
	case winrtWDDC::DisplayManagerResult::UnknownFailure: return "DisplayManagerResult::UnknownFailure";
	case winrtWDDC::DisplayManagerResult::TargetAccessDenied: return "DisplayManagerResult::TargetAccessDenied";
	case winrtWDDC::DisplayManagerResult::TargetStale: return "DisplayManagerResult::TargetStale";
	case winrtWDDC::DisplayManagerResult::RemoteSessionNotSupported:
		return "DisplayManagerResult::RemoteSessionNotSupported";
	}
	return "DisplayManagerResult::UNKNOWN";
}

constexpr const char *
to_string(winrtWDDC::DisplayStateOperationStatus e)
{
	switch (e) {
	case winrtWDDC::DisplayStateOperationStatus::Success: return "DisplayStateOperationStatus::Success";
	case winrtWDDC::DisplayStateOperationStatus::PartialFailure:
		return "DisplayStateOperationStatus::PartialFailure";
	case winrtWDDC::DisplayStateOperationStatus::UnknownFailure:
		return "DisplayStateOperationStatus::UnknownFailure";
	case winrtWDDC::DisplayStateOperationStatus::TargetOwnershipLost:
		return "DisplayStateOperationStatus::TargetOwnershipLost";
	case winrtWDDC::DisplayStateOperationStatus::SystemStateChanged:
		return "DisplayStateOperationStatus::SystemStateChanged";
	case winrtWDDC::DisplayStateOperationStatus::TooManyPathsForAdapter:
		return "DisplayStateOperationStatus::TooManyPathsForAdapter";
	case winrtWDDC::DisplayStateOperationStatus::ModesNotSupported:
		return "DisplayStateOperationStatus::ModesNotSupported";
	case winrtWDDC::DisplayStateOperationStatus::RemoteSessionNotSupported:
		return "DisplayStateOperationStatus::RemoteSessionNotSupported";
	}
	return "DisplayStateOperationStatus::UNKNOWN";
}

constexpr const char *
to_string(winrtWDDC::DisplayPathStatus e)
{
	switch (e) {
	case winrtWDDC::DisplayPathStatus::Unknown: return "DisplayPathStatus::Unknown";
	case winrtWDDC::DisplayPathStatus::Succeeded: return "DisplayPathStatus::Succeeded";
	case winrtWDDC::DisplayPathStatus::Pending: return "DisplayPathStatus::Pending";
	case winrtWDDC::DisplayPathStatus::Failed: return "DisplayPathStatus::Failed";
	case winrtWDDC::DisplayPathStatus::FailedAsync: return "DisplayPathStatus::FailedAsync";
	case winrtWDDC::DisplayPathStatus::InvalidatedAsync: return "DisplayPathStatus::InvalidatedAsync";
	}
	return "DisplayPathStatus::UNKNOWN";
}

/**
 * CreateSimpleScanout is prone to spurious IllegalArgument failures, so
 * this wrapper tries twice.
 *
 * See https://github.com/MicrosoftDocs/winrt-api/issues/1942
 *
 * @param device Display device
 * @param source Display source
 * @param primary Primary display surface
 * @param subResourceIndex as in CreateSimpleScanout. Usually 0 unless you are using hardware stereo (like 3D TV)
 * @param syncInterval as in CreateSimpleScanout
 * @return winrtWDDC::DisplayScanout
 */
static winrtWDDC::DisplayScanout
createScanout(winrtWDDC::DisplayDevice const &device,
              winrtWDDC::DisplaySource const &source,
              winrtWDDC::DisplaySurface const &primary,
              uint32_t subResourceIndex,
              uint32_t syncInterval)
{
	winrtWDDC::DisplayScanout ret{nullptr};
	try {
		ret = device.CreateSimpleScanout(source, primary, subResourceIndex, syncInterval);
	} catch (winrt::hresult_invalid_argument const &) {
		// ignore
	}
	if (ret == nullptr) {
		try {
			ret = device.CreateSimpleScanout(source, primary, subResourceIndex, syncInterval);
		} catch (winrt::hresult_invalid_argument const &) {
			// ignore
		}
	}

	if (ret == nullptr) {
		throw std::runtime_error("Couldn't construct a scanout even after two tries.");
	}
	return ret;
}
/// Things to pass between the opening of the device and the CompositorSwapchain constructor
using DisplayObjects =
    std::tuple<winrtWDDC::DisplayDevice, winrtWDDC::DisplayTarget, winrtWDDC::DisplayPath, winrtWDDC::DisplaySource>;

class CompositorSwapchain
{
public:
	CompositorSwapchain(struct comp_compositor *comp,
	                    DisplayObjects &&objects,
	                    winrt::Windows::Graphics::DirectX::DirectXColorSpace colorSpace,
	                    uint32_t numImages);
	CompositorSwapchain(struct comp_compositor *comp,
	                    const winrtWDDC::DisplayDevice &device,
	                    const winrtWDDC::DisplayTarget &target,
	                    const winrtWDDC::DisplayPath &path,
	                    winrtWDDC::DisplaySource &&source,
	                    winrt::Windows::Graphics::DirectX::DirectXColorSpace colorSpace,
	                    uint32_t numImages);

	CompositorSwapchain(CompositorSwapchain const &) = delete;
	CompositorSwapchain(CompositorSwapchain &&) = delete;
	CompositorSwapchain &
	operator=(CompositorSwapchain const &) = delete;
	CompositorSwapchain &
	operator=(CompositorSwapchain &&) = delete;


	HANDLE
	getSurfaceHandle(uint32_t i) const
	{
		return m_surfaceHandles[i].get();
	}

	uint32_t
	acquireNext();

	uint32_t
	getHeight() const
	{
		return (uint32_t)m_path.SourceResolution().Value().Height;
	}
	uint32_t
	getWidth() const
	{
		return (uint32_t)m_path.SourceResolution().Value().Width;
	}

	DXGI_FORMAT
	getFormat() const
	{
		return (DXGI_FORMAT)(static_cast<int>(m_path.SourcePixelFormat()));
	}


	winrtWDDC::DisplayRotation
	getSurfaceTransform() const
	{
		return m_path.Rotation();
	}

	bool
	present(uint32_t i, const winrtWDDC::DisplayFence &fence, uint64_t fenceValue);

private:
	/// The compositor that owns us
	struct comp_compositor *c;
	xrt::auxiliary::util::ComGuard com_guard;
	bool m_haveApi14{checkForApi14()};
	uint32_t m_nextToAcquire{0};

	winrtWDDC::DisplaySource m_source;
	winrtWDDC::DisplayTaskPool m_taskPool;
	winrtWDDC::DisplayPath m_path;
	std::vector<winrtWDDC::DisplaySurface> m_surfaces;
	std::vector<wil::unique_handle> m_surfaceHandles;
	std::vector<winrtWDDC::DisplayScanout> m_scanouts;
};

CompositorSwapchain::CompositorSwapchain(struct comp_compositor *comp,
                                         DisplayObjects &&objects,
                                         winrt::Windows::Graphics::DirectX::DirectXColorSpace colorSpace,
                                         uint32_t numImages)
    : CompositorSwapchain(comp,
                          std::get<winrtWDDC::DisplayDevice>(objects),
                          std::get<winrtWDDC::DisplayTarget>(objects),
                          std::get<winrtWDDC::DisplayPath>(objects),
                          std::move(std::get<winrtWDDC::DisplaySource>(objects)),
                          colorSpace,
                          numImages)
{}

inline CompositorSwapchain::CompositorSwapchain(struct comp_compositor *comp,
                                                const winrtWDDC::DisplayDevice &device,
                                                const winrtWDDC::DisplayTarget &target,
                                                const winrtWDDC::DisplayPath &path,
                                                winrtWDDC::DisplaySource &&source,
                                                winrt::Windows::Graphics::DirectX::DirectXColorSpace colorSpace,
                                                uint32_t numImages)

    : c(comp), m_source(std::move(source)), m_taskPool(device.CreateTaskPool()), m_path(path),
      m_surfaces(numImages, nullptr), m_surfaceHandles(numImages), m_scanouts(numImages, nullptr)
{
	winrt::Windows::Graphics::SizeInt32 const resolution = path.SourceResolution().Value();
	winrt::Windows::Graphics::DirectX::Direct3D11::Direct3DMultisampleDescription const multisample{1, 0};

	winrtWDDC::DisplayPrimaryDescription const primaryDescription(
	    (uint32_t)resolution.Width, (uint32_t)resolution.Height, path.SourcePixelFormat(), colorSpace,
	    /* isStereo */ false, multisample);
	auto deviceInterop = device.as<::IDisplayDeviceInterop>();

	for (uint32_t i = 0; i < numImages; ++i) {
		auto surface = device.CreatePrimary(target, primaryDescription);
		m_surfaces[i] = surface;
		auto surfaceInspectable = surface.as<::IInspectable>();
		THROW_IF_FAILED(deviceInterop->CreateSharedHandle(surfaceInspectable.get(), nullptr, GENERIC_ALL,
		                                                  nullptr, m_surfaceHandles[i].put()));
		m_scanouts[i] =
		    createScanout(device, m_source, m_surfaces[i], /* SubResourceIndex */ 0, /*SyncInterval */ 0);
	}
}

uint32_t
CompositorSwapchain::acquireNext()
{
	uint32_t const ret = m_nextToAcquire;
	m_nextToAcquire = (m_nextToAcquire + 1) % m_surfaces.size();
	return ret;
}

bool
CompositorSwapchain::present(uint32_t i, const winrtWDDC::DisplayFence &fence, uint64_t fenceValue)
{
	COMP_INFO(c, "Will scan out surface %" PRIu32 " after fence is signalled with %" PRIu64, i, fenceValue);
	auto task = m_taskPool.CreateTask();
	task.SetWait(fence, fenceValue);
	task.SetScanout(m_scanouts[i]);
	m_taskPool.ExecuteTask(task);
	if (m_path.Status() != winrtWDDC::DisplayPathStatus::Succeeded) {
		COMP_ERROR(c, "Path status is an error: %s", to_string(m_path.Status()));
		return false;
	}
	return true;
}

class CompTargetData
{
public:
	explicit CompTargetData(struct comp_compositor *comp);

	bool
	findHmds();

	bool
	openHmd(const winrtWDDC::DisplayTarget &target, DisplayObjects &outObjects) noexcept;

	bool
	targetPredicate(winrtWDDC::DisplayTarget const &target) const;

	/// The compositor that owns us
	struct comp_compositor *c;
	xrt::auxiliary::util::ComGuard com_guard;

	/// Whether we can/should use the Windows 11+-only APIs
	bool useApi14;

	winrtWDDC::DisplayManager manager;

	/// @name DXGI/D3D11 objects
	/// @{
	wil::com_ptr<IDXGIAdapter> dxgiAdapter;
	wil::com_ptr<ID3D11Device5> d3d11Device;
	wil::com_ptr<ID3D11DeviceContext4> d3d11Context;
	LUID luid;

	wil::com_ptr<ID3D11Fence> d3d11RenderCompleteFence;
	/// @}

	wil::unique_handle renderCompleteFenceHandle;

	/// @name WinRT objects
	/// @{
	winrtWDDC::DisplayAdapter displayAdapter{nullptr};
	winrtWDDC::DisplayDevice displayDevice{nullptr};

	winrtWDDC::DisplayFence renderCompleteFence{nullptr};
	std::vector<winrtWDDC::DisplayTarget> hmds;

	DisplayObjects objects{nullptr, nullptr, nullptr, nullptr};

	// winrtWDDC::DisplayTarget displayTarget{nullptr};
	// winrtWDDC::DisplayPath displayPath{nullptr};
	// winrtWDDC::DisplayState displayState{nullptr};
	/// @}
};


CompTargetData::CompTargetData(struct comp_compositor *comp)
    : c(comp), useApi14(checkForApi14()),
      manager(winrtWDDC::DisplayManager::Create(winrtWDDC::DisplayManagerOptions::EnforceSourceOwnership)),
      dxgiAdapter(xrt::auxiliary::d3d::getAdapterByLUID(c->settings.client_gpu_deviceLUID))
{
	assert(c->settings.client_gpu_deviceLUID_valid);

	// Get some D3D11 stuff mainly for fence handling
	{
		wil::com_ptr<ID3D11Device> our_dev;
		wil::com_ptr<ID3D11DeviceContext> our_context;
		std::tie(our_dev, our_context) =
		    xrt::auxiliary::d3d::d3d11::createDevice(dxgiAdapter, c->settings.log_level);
		our_dev.query_to(d3d11Device.put());
		our_context.query_to(d3d11Context.put());

		THROW_IF_FAILED(
		    d3d11Device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(d3d11RenderCompleteFence.put())));
	}

	// Get the LUID in Windows format
	{
		DXGI_ADAPTER_DESC desc{};
		THROW_IF_FAILED(dxgiAdapter->GetDesc(&desc));
		luid = desc.AdapterLuid;
	}

	// get the adapter and device for winrt
	{
		winrt::Windows::Graphics::DisplayAdapterId id{};
		id.LowPart = luid.LowPart;
		id.HighPart = luid.HighPart;
		displayAdapter = winrtWDDC::DisplayAdapter::FromId(id);
		if (displayAdapter == nullptr) {
			throw std::runtime_error("Could not get adapter by ID in WinRT!");
		}
		THROW_LAST_ERROR_IF_NULL(displayAdapter);
		displayDevice = manager.CreateDisplayDevice(displayAdapter);
	}
	// Get the handle for the fence
	{
		wil::unique_handle fenceHandle;
		THROW_IF_FAILED(
		    d3d11RenderCompleteFence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, fenceHandle.put()));
		renderCompleteFenceHandle = std::move(fenceHandle);
	}
	// get the winrt object for the fence
	{
		winrt::com_ptr<::IInspectable> fence;
		THROW_IF_FAILED(displayDevice.as<IDisplayDeviceInterop>()->OpenSharedHandle(
		    renderCompleteFenceHandle.get(), IID_PPV_ARGS(fence.put())));
		renderCompleteFence = fence.as<winrtWDDC::DisplayFence>();
	}
}

bool
CompTargetData::targetPredicate(winrtWDDC::DisplayTarget const &target) const
{
	try {
		if (target == nullptr) {
			COMP_WARN(c, "Skipping target because it's NULL.");
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
		LUID const thisLuid = {adapter.Id().LowPart, adapter.Id().HighPart};

		if (thisLuid.LowPart != luid.LowPart || thisLuid.HighPart != luid.HighPart) {
			COMP_INFO(c, "Skipping target because LUID doesn't match.");
			return false;
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
bool
CompTargetData::findHmds()
{
	using std::begin;
	using std::end;
	hmds.clear();

	auto currentTargets = manager.GetCurrentTargets();

	COMP_INFO(c, "About to filter targets: starting with %d", int(currentTargets.Size()));
	std::copy_if(begin(currentTargets), end(currentTargets), std::back_inserter(hmds),
	             [&](winrtWDDC::DisplayTarget const &target) { return this->targetPredicate(target); });
	COMP_INFO(c, "Filtering left us with %d possible HMD targets", int(hmds.size()));

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
			// this path object is just temporary, we can get it back later if everything worked right
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
	for (int attempt = 0; attempt < kMaxOpenAttempts; ++attempt) {
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
	winrt::init_apartment();
	try {

		ctdw->data = std::make_unique<CompTargetData>(ct->c);


		if (!ctdw->data->findHmds()) {
			COMP_INFO(
			    ct->c,
			    "No displays with headset EDID flag set are available: cannot use Windows direct mode.");
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
		if (!checkForBasicAPI()) {
			// Cannot use this API on this Windows version
			return nullptr;
		}

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

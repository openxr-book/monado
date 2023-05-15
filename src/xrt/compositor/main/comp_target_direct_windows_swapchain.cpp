// Copyright 2019-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Windows 10/11 direct mode code.
 *
 * Has to implement comp_target instead of comp_target_swapchain because
 * we don't get a VkSurfaceKHR, etc: we manually import images instead.
 *
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup comp_main
 */

#include "comp_target_direct_windows_swapchain.hpp"

#include "d3d/d3d_winrt_helpers.hpp"

#include <Inspectable.h>
#include <windows.devices.display.core.interop.h>
#include <inttypes.h>
#include <winrt/impl/Windows.Devices.Display.Core.0.h>
#include <winrt/impl/Windows.Devices.Display.Core.2.h>

namespace xrt::compositor::main {
using namespace xrt::auxiliary::d3d::winrt;
using ::winrt::Windows::Devices::Display::Core::DisplayDevice;
using ::winrt::Windows::Devices::Display::Core::DisplayFence;
using ::winrt::Windows::Devices::Display::Core::DisplayPath;
using ::winrt::Windows::Devices::Display::Core::DisplayPresentStatus;
using ::winrt::Windows::Devices::Display::Core::DisplayPrimaryDescription;
using ::winrt::Windows::Devices::Display::Core::DisplaySource;
using ::winrt::Windows::Devices::Display::Core::DisplayTarget;

/// We retry opening an HMD a few times since it sometimes fails spuriously
constexpr int kMaxOpenAttempts = 2;

CompositorSwapchain::CompositorSwapchain(struct comp_compositor *comp,
                                         DisplayObjects &&objects,
                                         winrt::Windows::Graphics::DirectX::DirectXColorSpace colorSpace,
                                         uint32_t numImages)
    : CompositorSwapchain(comp,
                          std::get<DisplayDevice>(objects),
                          std::get<DisplayTarget>(objects),
                          std::get<DisplayPath>(objects),
                          std::move(std::get<DisplaySource>(objects)),
                          colorSpace,
                          numImages)
{}

CompositorSwapchain::CompositorSwapchain(struct comp_compositor *comp,
                                         const DisplayDevice &device,
                                         const DisplayTarget &target,
                                         const DisplayPath &path,
                                         DisplaySource &&source,
                                         winrt::Windows::Graphics::DirectX::DirectXColorSpace colorSpace,
                                         uint32_t numImages)

    : c(comp), m_source(std::move(source)), m_taskPool(device.CreateTaskPool()), m_path(path),
      m_surfaces(numImages, nullptr), m_surfaceHandles(numImages), m_scanouts(numImages, nullptr)
{
	m_capability.populate();
	winrt::Windows::Graphics::SizeInt32 const resolution = path.SourceResolution().Value();
	winrt::Windows::Graphics::DirectX::Direct3D11::Direct3DMultisampleDescription const multisample{1, 0};

	DisplayPrimaryDescription const primaryDescription((uint32_t)resolution.Width, (uint32_t)resolution.Height,
	                                                   path.SourcePixelFormat(), colorSpace,
	                                                   /* isStereo */ false, multisample);
	auto deviceInterop = device.as<::IDisplayDeviceInterop>();

	for (uint32_t i = 0; i < numImages; ++i) {
		auto surface = device.CreatePrimary(target, primaryDescription);
		m_surfaces[i] = surface;
		auto surfaceInspectable = surface.as<::IInspectable>();
		THROW_IF_FAILED(deviceInterop->CreateSharedHandle(surfaceInspectable.get(), nullptr, GENERIC_ALL,
		                                                  nullptr, m_surfaceHandles[i].put()));
		//! @todo debug var for allow tearing
		m_scanouts[i] = auxiliary::d3d::winrt::createScanout(
		    m_capability, kMaxOpenAttempts, device, m_source, m_surfaces[i],
		    /* SubResourceIndex */ 0, /* allowTearing */ false);
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
CompositorSwapchain::present(uint32_t i, const DisplayFence &fence, uint64_t fenceValue)
{
	COMP_INFO(c, "Will scan out surface %" PRIu32 " after fence is signalled with %" PRIu64, i, fenceValue);
	auto task = m_taskPool.CreateTask();
	task.SetWait(fence, fenceValue);
	task.SetScanout(m_scanouts[i]);
	if (m_capability.supportsScanoutOptionsAndTryExecuteTask) {
		auto taskResult = m_taskPool.TryExecuteTask(task);
		m_presentId = taskResult.PresentId();

		auto status = taskResult.PresentStatus();
		if (status != DisplayPresentStatus::Success) {
			COMP_ERROR(c, "Display present status non-success: %s", to_string(status));
		}
	} else {
		m_taskPool.ExecuteTask(task);
	}
	if (m_path.Status() != winrtWDDC::DisplayPathStatus::Succeeded) {
		COMP_ERROR(c, "Path status is an error: %s", to_string(m_path.Status()));
		return false;
	}
	return true;
}
} // namespace xrt::compositor::main

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
#pragma once

#include "comp_compositor.h"

#include "xrt/xrt_windows.h"
#include "util/u_win32_com_guard.hpp"
#include "d3d/d3d_winrt_helpers.hpp"

#include "dxgiformat.h"

#include <stdint.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Devices.Display.h>
#include <winrt/Windows.Graphics.DirectX.h>

#include <wil/resource.h>
namespace xrt::compositor::main {

namespace winrtWDDC = winrt::Windows::Devices::Display::Core;

/// Things to pass between the opening of the device and the CompositorSwapchain constructor
using DisplayObjects =
    std::tuple<winrtWDDC::DisplayDevice, winrtWDDC::DisplayTarget, winrtWDDC::DisplayPath, winrtWDDC::DisplaySource>;

/**
 * Roughly emulate the functionality of a VkSwapchain with a collection of WinRT
 * display primaries.
 */
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
	auxiliary::d3d::winrt::SystemApiCapability m_capability{};
	auxiliary::util::ComGuard com_guard;
	uint32_t m_nextToAcquire{0};

	/// The PresentId returned from TryExecuteTask in Win11, if available.
	/// For use in waiting on fences/events
	uint64_t m_presentId{static_cast<uint64_t>(-1)};

	winrtWDDC::DisplaySource m_source;
	winrtWDDC::DisplayTaskPool m_taskPool;
	winrtWDDC::DisplayPath m_path;
	std::vector<winrtWDDC::DisplaySurface> m_surfaces;
	std::vector<wil::unique_handle> m_surfaceHandles;
	std::vector<winrtWDDC::DisplayScanout> m_scanouts;
};
} // namespace xrt::compositor::main

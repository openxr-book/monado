// Copyright 2019-2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Helpers for Windows 10+ direct mode code.
 *
 * Has to implement comp_target instead of comp_target_swapchain because
 * we don't get a VkSurfaceKHR, etc: we manually import images instead.
 *
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup aux_d3d
 */

#pragma once

#include "math/m_rational.hpp"

#include "xrt/xrt_vulkan_includes.h"

#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Devices.Display.Core.h>

#include <stdint.h>
#include <utility>
#include <tuple>

namespace xrt::auxiliary::d3d::winrt {


// struct DisplayModeData
// {
// 	winrt::Windows::Graphics::DirectX::DirectXPixelFormat format;
// 	math::Rational<uint32_t> verticalRefresh;
// 	winrt::Windows::Devices::Display::Core::DisplayModeInfo
// };

static inline xrt::auxiliary::math::Rational<uint32_t>
parseVerticalRefreshRate(const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &modeInfo)
{
	return xrt::auxiliary::math::Rational<uint32_t>{modeInfo.PresentationRate().VerticalSyncRate.Numerator,
	                                                modeInfo.PresentationRate().VerticalSyncRate.Denominator};
}

bool
isModeAcceptable(const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &modeInfo);

/**
 * A comparison function (like std::less) for comparing display modes, such as for
 * sorting or finding the "best" one.
 *
 * This sorts with SRGB modes first, and refresh rate in decreasing order.
 */
bool
modeComparison(const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &lhs,
               const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &rhs);

// /**
//  * Sort a vector of *acceptable* display modes,
//  *
//  * @param[in,out] acceptableModes Some acceptable modes (according to @ref isModeAcceptable), that will be sorted in
//  * place.
//  */
// void
// sortModes(std::vector<::winrt::Windows::Devices::Display::Core::DisplayModeInfo> &acceptableModes);

static inline bool
colorSpaceFromVulkan(VkColorSpaceKHR colorSpace, ::winrt::Windows::Graphics::DirectX::DirectXColorSpace &out)
{
	using ::winrt::Windows::Graphics::DirectX::DirectXColorSpace;
	switch (colorSpace) {
	case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: out = DirectXColorSpace::RgbFullG22NoneP709; return true;
	case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: out = DirectXColorSpace::RgbFullG10NoneP709; return true;
	}
	return false;
}
} // namespace xrt::auxiliary::d3d::winrt

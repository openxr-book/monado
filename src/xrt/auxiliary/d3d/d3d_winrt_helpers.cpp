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
#include "d3d_winrt_helpers.hpp"

#include <dxgiformat.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Devices.Display.Core.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <windows.devices.display.core.interop.h>

#include <d3d11_1.h>
#include <d3d11_4.h>

#include <wil/com.h> // must be after winrt
#include <wil/resource.h>
#include <wil/result_macros.h>

#include <array>
#include <tuple>
#include <algorithm>
#include <utility>

namespace xrt::auxiliary::d3d::winrt {

namespace winrtWDDC = ::winrt::Windows::Devices::Display::Core;
namespace winrtWDD = ::winrt::Windows::Devices::Display;
namespace Collections = ::winrt::Windows::Foundation::Collections;
using ::winrt::Windows::Graphics::DirectX::DirectXPixelFormat;

// static constexpr std::array<std::pair<DirectXPixelFormat, DXGI_FORMAT>, 6> kSurfaceFormats = {
static inline std::tuple<bool, uint16_t, DXGI_FORMAT>
lookupFormat(DirectXPixelFormat format)
{
	switch (format) {
	// These are all first-tier
	case DirectXPixelFormat::B8G8R8A8UIntNormalizedSrgb: return {true, 0, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB};
	case DirectXPixelFormat::B8G8R8X8UIntNormalizedSrgb: return {true, 0, DXGI_FORMAT_B8G8R8X8_UNORM_SRGB};
	case DirectXPixelFormat::R8G8B8A8UIntNormalizedSrgb: return {true, 0, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
	// These are second tier options
	case DirectXPixelFormat::B8G8R8A8UIntNormalized: return {true, 5, DXGI_FORMAT_B8G8R8A8_UNORM};
	case DirectXPixelFormat::B8G8R8X8UIntNormalized: return {true, 5, DXGI_FORMAT_B8G8R8X8_UNORM};
	case DirectXPixelFormat::R8G8B8A8UIntNormalized: return {true, 5, DXGI_FORMAT_R8G8B8A8_UNORM};
	default: return {false, 1000, (DXGI_FORMAT)0};
	}
}

/// Make something sortable out of a display mode
static inline std::pair<uint16_t, float>
makeModeKey(const winrtWDDC::DisplayModeInfo &modeInfo)
{
	auto format = modeInfo.SourcePixelFormat();
	uint16_t preference = 5000;

	std::tie(std::ignore, preference, std::ignore) = lookupFormat(modeInfo.SourcePixelFormat());

	// negate the fps so we get the largest at the top, not the smallest
	return std::make_pair(preference, -parseVerticalRefreshRate(modeInfo).as_float());
}

// winrtWDDC::DisplayModeInfo
// pickPreferredMode(struct comp_compositor *c,
//                   ::winrt::Windows::Foundation::Collections::IVectorView<winrtWDDC::DisplayModeInfo> modes)
// {
// 	struct SortableMode
// 	{
// 		std::pair<ptrdiff_t, float> key;
// 		winrtWDDC::DisplayModeInfo mode;
// 	};
// 	std::vector<SortableMode> sortableModes;
// 	for (const auto &mode : modes) {

// 		std::pair<ptrdiff_t, float> key{};
// 		if (makeModeKey(mode, key)) {
// 			COMP_INFO(c, "Mode: Format preference %d, refresh %f", int(key.first), -key.second);
// 			sortableModes.emplace_back(key, mode);
// 		}
// 	}
// 	if (sortableModes.empty()) {
// 		return {};
// 	}
// 	std::sort(begin(sortableModes), end(sortableModes),
// 	          [](const SortableMode &a, const SortableMode &b) { return a.key < b.key; });
// 	return sortableModes[0].mode;
// }

bool
isModeAcceptable(const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &modeInfo)
{
	bool ret = false;
	std::tie(ret, std::ignore, std::ignore) = lookupFormat(modeInfo.SourcePixelFormat());
	return ret;
}
bool
modeComparison(const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &lhs,
               const ::winrt::Windows::Devices::Display::Core::DisplayModeInfo &rhs)
{
	return makeModeKey(lhs) < makeModeKey(rhs);
}

void
sortModes(std::vector<::winrt::Windows::Devices::Display::Core::DisplayModeInfo> &acceptableModes)
{
	std::sort(begin(acceptableModes), end(acceptableModes), modeComparison);
}

} // namespace xrt::auxiliary::d3d::winrt

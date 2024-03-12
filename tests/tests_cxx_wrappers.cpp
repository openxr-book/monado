// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Miscellanous C++ wrapper tests.
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 */

#include <iostream>
#include <xrt/xrt_device.hpp>

#include "catch/catch.hpp"


struct silly_device
{
	xrt_device base{};
	bool *destroyed;


	silly_device(bool &destroyed_) : destroyed(&destroyed_)
	{
		static const struct xrt_device_interface silly_impl = {
			"silly",
			// destroy
			[](xrt_device *xdev) { delete reinterpret_cast<silly_device *>(xdev); },
			nullptr, // update_inputs
			nullptr, // get_tracked_pose
			nullptr, // get_hand_tracking
			nullptr, // get_face_tracking
			nullptr, // set_output
			nullptr, // get_view_poses
			nullptr, // compute_distortion
			nullptr, // get_visibility_mask
			nullptr, // ref_space_usage
			nullptr, // is_form_factor_available
		};

		base.impl = &silly_impl;
	}
	~silly_device()
	{
		*destroyed = true;
	}
};

TEST_CASE("unique_xrt_device")
{

	bool destroyed = false;
	{
		// make the device
		auto specific = std::make_unique<silly_device>(destroyed);
		CHECK_FALSE(destroyed);

		// use the generic unique_ptr
		xrt::unique_xrt_device generic(&(specific.release()->base));
		CHECK_FALSE(destroyed);
	}
	// make sure it went away
	CHECK(destroyed);
}

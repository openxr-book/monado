// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Android surface callback collection tests.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 */

#include "catch/catch.hpp"

#include <android/android_surface_callbacks.h>

static bool
increment_userdata_int(struct xrt_instance *xinst,
                       struct _ANativeWindow *window,
                       enum xrt_android_surface_event event,
                       void *userdata)
{
	*static_cast<int *>(userdata) += 1;
	return true;
}

TEST_CASE("android_surface_callbacks")
{
	// fake pointers
	xrt_instance *xinst = reinterpret_cast<xrt_instance *>(0x01);
	_ANativeWindow *window = reinterpret_cast<_ANativeWindow *>(0x02);
	android_surface_callbacks *asc = android_surface_callbacks_create(xinst);
	CHECK(asc != nullptr);
	SECTION("call when empty")
	{
		CHECK(0 == android_surface_callbacks_invoke(asc, window, XRT_ANDROID_SURFACE_EVENT_ACQUIRED));
		CHECK(0 == android_surface_callbacks_invoke(asc, window, XRT_ANDROID_SURFACE_EVENT_LOST));
		CHECK(0 == android_surface_callbacks_remove_callback(asc, &increment_userdata_int,
		                                                     XRT_ANDROID_SURFACE_EVENT_LOST, nullptr));
	}
	SECTION("same function, different mask and userdata")
	{
		int numAcquired = 0;
		int numLost = 0;
		REQUIRE(0 == android_surface_callbacks_register_callback(
		                 asc, increment_userdata_int, XRT_ANDROID_SURFACE_EVENT_ACQUIRED, &numAcquired));
		REQUIRE(0 == android_surface_callbacks_register_callback(asc, increment_userdata_int,
		                                                         XRT_ANDROID_SURFACE_EVENT_LOST, &numLost));
		SECTION("removal matching")
		{
			CHECK(0 == android_surface_callbacks_remove_callback(
			               asc, &increment_userdata_int, XRT_ANDROID_SURFACE_EVENT_LOST, &numAcquired));
			CHECK(0 == android_surface_callbacks_remove_callback(
			               asc, &increment_userdata_int, XRT_ANDROID_SURFACE_EVENT_ACQUIRED, &numLost));
		}
		CHECK(1 == android_surface_callbacks_remove_callback(asc, &increment_userdata_int,
		                                                     XRT_ANDROID_SURFACE_EVENT_LOST, &numLost));
	}

	android_surface_callbacks_destroy(&asc);
	CHECK(asc == nullptr);
}

// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementation of a callback collection for Android surfaces.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup aux_android
 */

#include "android_surface_callbacks.h"
#include "xrt/xrt_config_android.h"
#include "xrt/xrt_instance.h"
#include "xrt/xrt_android.h"
#include "util/u_logging.h"
#include "util/u_generic_callbacks.hpp"

#include <memory>

struct android_surface_callbacks
{
	explicit android_surface_callbacks(xrt_instance *xinst) : instance(xinst) {}
	xrt_instance *instance;
	u_generic_callbacks<xrt_android_surface_event_handler_t, enum xrt_android_surface_event> callback_collection;
};

#define CATCH_CLAUSES(ACTION, RET)                                                                                     \
	catch (std::exception const &e)                                                                                \
	{                                                                                                              \
		U_LOG_E("Exception while " ACTION "! %s", e.what());                                                   \
		return RET;                                                                                            \
	}                                                                                                              \
	catch (...)                                                                                                    \
	{                                                                                                              \
		U_LOG_E("Unknown exception while " ACTION "!");                                                        \
		return RET;                                                                                            \
	}

int
android_surface_callbacks_register_callback(struct android_surface_callbacks *asc,
                                            xrt_android_surface_event_handler_t callback,
                                            enum xrt_android_surface_event event_mask,
                                            void *userdata)
{
	try {
		asc->callback_collection.addCallback(callback, event_mask, userdata);
		return 0;
	}
	CATCH_CLAUSES("adding callback to collection", -1)
}

int
android_surface_callbacks_remove_callback(struct android_surface_callbacks *asc,
                                          xrt_android_surface_event_handler_t callback,
                                          enum xrt_android_surface_event event_mask,
                                          void *userdata)
{
	try {
		return asc->callback_collection.removeCallback(callback, event_mask, userdata);
	}
	CATCH_CLAUSES("removing callback", -1)
}

int
android_surface_callbacks_invoke(struct android_surface_callbacks *asc,
                                 struct _ANativeWindow *window,
                                 enum xrt_android_surface_event event)
{
	try {
		return asc->callback_collection.invokeCallbacks(
		    event, [=](enum xrt_android_surface_event event, xrt_android_surface_event_handler_t callback,
		               void *userdata) { return callback(asc->instance, window, event, userdata); });
	}
	CATCH_CLAUSES("invoking callbacks", -1)
}

struct android_surface_callbacks *
android_surface_callbacks_create(struct xrt_instance *xinst)
{
	try {
		auto ret = std::make_unique<android_surface_callbacks>(xinst);

		return ret.release();
	}
	CATCH_CLAUSES("creating callbacks structure", nullptr)
}

void
android_surface_callbacks_destroy(struct android_surface_callbacks **ptr_callbacks)
{
	if (ptr_callbacks == nullptr) {
		return;
	}
	struct android_surface_callbacks *asc = *ptr_callbacks;
	if (asc == nullptr) {
		return;
	}
	delete asc;
	*ptr_callbacks = nullptr;
}

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

#include <vector>
#include <algorithm>
#include <tuple>
#include <memory>

struct android_surface_callback_entry
{
	xrt_android_surface_event_handler_t callback;
	enum xrt_android_surface_event event_mask;
	void *userdata;
	bool should_remove = false;

	android_surface_callback_entry(xrt_android_surface_event_handler_t callback_,
	                               enum xrt_android_surface_event event_mask_,
	                               void *userdata_)
	    : callback(callback_), event_mask(event_mask_), userdata(userdata_)
	{}

	bool
	matches(android_surface_callback_entry const &other) const;

	/*!
	 * Returns a pair {1 if we invoked else 0, will this callback be removed}
	 */
	std::pair<int, bool>
	maybeInvoke(struct xrt_instance *xinst, struct _ANativeWindow *window, enum xrt_android_surface_event event);
};

std::pair<int, bool>
android_surface_callback_entry::maybeInvoke(struct xrt_instance *xinst,
                                            struct _ANativeWindow *window,
                                            enum xrt_android_surface_event event)
{
	if ((event_mask & event) == 0) {
		return {0, false};
	}
	// ok, this is an event we should forward.
	auto result = callback(xinst, window, event, userdata);
	// flag for removal
	should_remove = !result;
	return {1, should_remove};
}

bool
android_surface_callback_entry::matches(android_surface_callback_entry const &other) const
{
	return callback == other.callback && event_mask == other.event_mask && userdata == other.userdata;
}

struct android_surface_callbacks
{
private:
	xrt_instance *instance;
	std::vector<android_surface_callback_entry> callbacks;

	int
	purgeMarkedCallbacks()
	{
		auto b = callbacks.begin();
		auto e = callbacks.end();
		auto new_end = std::remove_if(
		    b, e, [](android_surface_callback_entry const &entry) { return entry.should_remove; });
		auto num_removed = std::distance(new_end, e);
		callbacks.erase(new_end, e);
		return num_removed;
	}

public:
	explicit android_surface_callbacks(struct xrt_instance *xinst) : instance(xinst) {}

	void
	addCallback(xrt_android_surface_event_handler_t callback,
	            enum xrt_android_surface_event event_mask,
	            void *userdata)
	{
		callbacks.emplace_back(callback, event_mask, userdata);
	}

	int
	removeCallback(xrt_android_surface_event_handler_t callback,
	               enum xrt_android_surface_event event_mask,
	               void *userdata)
	{
		bool found = false;

		const android_surface_callback_entry needle{callback, event_mask, userdata};
		for (auto &entry : callbacks) {
			if (entry.matches(needle)) {
				entry.should_remove = true;
				found = true;
				// don't stop just in case somebody registered multiple times,
				// since we don't forbid that.
			}
		}
		if (found) {
			return purgeMarkedCallbacks();
		}
		return 0;
	}

	int
	invokeCallbacks(struct _ANativeWindow *window, enum xrt_android_surface_event event)
	{
		bool needPurge = false;

		int ran = 0;
		for (auto &entry : callbacks) {
			int ranThisCallback;
			bool willRemove;
			std::tie(ranThisCallback, willRemove) = entry.maybeInvoke(instance, window, event);
			needPurge |= willRemove;
			ran += ranThisCallback;
		}
		if (needPurge) {
			purgeMarkedCallbacks();
		}
		return ran;
	}
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
		asc->addCallback(callback, event_mask, userdata);
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
		return asc->removeCallback(callback, event_mask, userdata);
	}
	CATCH_CLAUSES("removing callback", -1)
}

int
android_surface_callbacks_invoke(struct android_surface_callbacks *asc,
                                 struct _ANativeWindow *window,
                                 enum xrt_android_surface_event event)
{
	try {
		return asc->invokeCallbacks(window, event);
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

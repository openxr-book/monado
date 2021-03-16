// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Header holding Android-specific instance methods.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt_config_os.h"
#include <stdbool.h>

// #ifdef XRT_OS_ANDROID

#ifdef __cplusplus
extern "C" {
#endif

struct _JavaVM;
struct _ANativeWindow;

/*!
 * Distinguishes the possible Android surface events from each other.
 *
 * Used as a bitmask when registering for callbacks.
 */
enum xrt_android_surface_event
{
	XRT_ANDROID_SURFACE_EVENT_ACQUIRED = 1 << 0,
	XRT_ANDROID_SURFACE_EVENT_LOST = 1 << 1,
};

/*!
 * A callback type for a handler of Android surface/window events.
 *
 * Return false to be removed from the callback list.
 */
typedef bool (*xrt_android_surface_event_handler_t)(struct xrt_instance *xinst,
                                                    struct _ANativeWindow *window,
                                                    enum xrt_android_surface_event event,
                                                    void *userdata);

#ifdef XRT_OS_ANDROID

/*!
 * @interface xrt_instance_android
 *
 * This is an extension of the @ref xrt_instance interface that is used only on Android.
 *
 * @sa ipc_instance_create
 */
struct xrt_instance_android
{

	/*!
	 * @name Interface Methods
	 *
	 * All Android-based implementations of the xrt_instance interface must additionally populate all these function
	 * pointers with their implementation methods. To use this interface, see the helper functions.
	 * @{
	 */
	/*!
	 * Store the Java VM instance pointer.
	 *
	 * @note Code consuming this interface should use xrt_instance_android_store_vm()
	 *
	 * @param xinst Pointer to self
	 *
	 * @return 0 on success, <0 on error.
	 */
	int (*store_vm)(struct xrt_instance *xinst, struct _JavaVM *vm);

	/*!
	 * Retrieve the stored Java VM instance pointer.
	 *
	 * @note Code consuming this interface should use xrt_instance_android_get_vm()
	 *
	 * @param xinst Pointer to self
	 *
	 * @return The VM pointer, if stored, otherwise null.
	 */
	struct _JavaVM *(*get_vm)(struct xrt_instance *xinst);
	/*!
	 * Store an activity android.content.Context jobject.
	 *
	 * @note Code consuming this interface should use xrt_instance_android_store_context()
	 *
	 * @param xinst Pointer to self
	 *
	 * @return 0 on success, <0 on error.
	 */
	int (*store_context)(struct xrt_instance *xinst, void *context);

	/*!
	 * Retrieve the stored activity android.content.Context jobject.
	 *
	 * For usage, cast the return value to jobject - a typedef whose definition
	 * differs between C (a void *) and C++ (a pointer to an empty class)
	 *
	 * @note Code consuming this interface should use xrt_instance_android_get_context()
	 *
	 * @param xinst Pointer to self
	 *
	 * @return The activity context, if stored, otherwise null.
	 */
	void *(*get_context)(struct xrt_instance *xinst);

	/*!
	 * Register a surface event callback.
	 *
	 * @note Code consuming this interface should use xrt_instance_android_register_surface_callback()
	 *
	 * @param xinst Pointer to self
	 * @param callback Function pointer for callback
	 * @param event_mask bitwise-OR of one or more values from @ref xrt_android_surface_event
	 * @param userdata An opaque pointer for use by the callback. Whatever you pass here will be passed to the
	 * callback when invoked.
	 *
	 * @return 0 on success, <0 on error.
	 */
	int (*register_surface_callback)(struct xrt_instance *xinst,
	                                 xrt_android_surface_event_handler_t callback,
	                                 enum xrt_android_surface_event event_mask,
	                                 void *userdata);

	/*!
	 * Remove a surface event callback that matches the supplied parameters.
	 *
	 * @note Code consuming this interface should use xrt_instance_android_remove_surface_callback()
	 *
	 * @param xinst Pointer to self
	 * @param callback Function pointer for callback
	 * @param event_mask bitwise-OR of one or more values from @ref xrt_android_surface_event
	 * @param userdata An opaque pointer for use by the callback. Whatever you pass here will be passed to the
	 * callback when invoked.
	 *
	 * @return 0 on success, <0 on error.
	 */
	int (*remove_surface_callback)(struct xrt_instance *xinst,
	                               xrt_android_surface_event_handler_t callback,
	                               enum xrt_android_surface_event event_mask,
	                               void *userdata);
	/*!
	 * @}
	 */
};

#endif // XRT_OS_ANDROID

#ifdef __cplusplus
}
#endif

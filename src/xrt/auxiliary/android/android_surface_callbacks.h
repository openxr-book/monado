// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  An implementation of a callback collection for Android surfaces.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup aux_android
 */

#pragma once

#include <xrt/xrt_config_os.h>
#include <xrt/xrt_android.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @class android_surface_callbacks
 * @brief An object handling a collection of Android surface callbacks.
 */
struct android_surface_callbacks;

/*!
 * Create an @ref android_surface_callbacks object.
 *
 * @param xinst The instance that will be passed to all callbacks.
 *
 * @public @memberof android_surface_callbacks
 */
struct android_surface_callbacks *
android_surface_callbacks_create(struct xrt_instance *xinst);

/*!
 * Destroy an @ref android_surface_callbacks object.
 * @public @memberof android_surface_callbacks
 */
void
android_surface_callbacks_destroy(struct android_surface_callbacks **ptr_callbacks);

/*!
 * Register a surface event callback.
 *
 * @param asc Pointer to self
 * @param callback Function pointer for callback
 * @param event_mask bitwise-OR of one or more values from @ref xrt_android_surface_event
 * @param userdata An opaque pointer for use by the callback. Whatever you pass here will be passed to the
 * callback when invoked.
 *
 * @return 0 on success, <0 on error.
 * @public @memberof android_surface_callbacks
 */
int
android_surface_callbacks_register_callback(struct android_surface_callbacks *asc,
                                            xrt_android_surface_event_handler_t callback,
                                            enum xrt_android_surface_event event_mask,
                                            void *userdata);

/*!
 * Remove a surface event callback that matches the supplied parameters.
 *
 * @param asc Pointer to self
 * @param callback Function pointer for callback
 * @param event_mask bitwise-OR of one or more values from @ref xrt_android_surface_event
 * @param userdata An opaque pointer for use by the callback. Whatever you pass here will be passed to the
 * callback when invoked.
 *
 * @return number of callbacks removed (typically 1) on success, <0 on error.
 * @public @memberof android_surface_callbacks
 */
int
android_surface_callbacks_remove_callback(struct android_surface_callbacks *asc,
                                          xrt_android_surface_event_handler_t callback,
                                          enum xrt_android_surface_event event_mask,
                                          void *userdata);


/*!
 * Invoke all surface event callbacks that match a given event.
 *
 * @param asc Pointer to self
 * @param window The relevant window/surface
 * @param event The event from @ref xrt_android_surface_event
 *
 * @return the number of invoked callbacks on success, <0 on error.
 * @public @memberof android_surface_callbacks
 */
int
android_surface_callbacks_invoke(struct android_surface_callbacks *asc,
                                 struct _ANativeWindow *window,
                                 enum xrt_android_surface_event event);


#ifdef __cplusplus
}
#endif

// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Functions for adding a new Surface to a window and otherwise
 *         interacting with an Android View.
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup aux_android
 */

#pragma once

#include <xrt/xrt_config_os.h>
#include <xrt/xrt_limits.h>

#ifdef XRT_OS_ANDROID

#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

struct _JNIEnv;
struct _JavaVM;

struct xrt_android_display_metrics
{
	int width_pixels;
	int height_pixels;
	int density_dpi;
	float density;
	float scaled_density;
	float xdpi;
	float ydpi;
	float refresh_rate;
	float refresh_rates[XRT_MAX_SUPPORTED_REFRESH_RATES];
	uint32_t refresh_rate_count;
};

/*!
 * Opaque type representing a custom surface added to a window, and the async
 * operation to perform this adding.
 *
 * @note You must keep this around for as long as you're using the surface.
 */
struct android_custom_surface;

/*!
 * Start adding a custom surface to a window.
 *
 * This is an asynchronous operation, so this creates an opaque pointer for you
 * to check on the results and maintain a reference to the result.
 *
 * Uses org.freedesktop.monado.auxiliary.MonadoView
 *
 * @param vm Java VM pointer
 * @param context An android.content.Context jobject, cast to `void *`.
 * @param display_id ID of the display that the surface is attached to.
 * @param surface_title Title of the surface.
 * @param preferred_display_mode_id The preferred display mode ID.
 *        A value of 0 indicates no preference.
 *        Non-zero values map to the corresponding display mode
 *        ID that are returned from the getSupportedModes() method for
 *        the given Android display. (the 1-indexed IDs.)
 *
 * @return An opaque handle for monitoring this operation and referencing the
 * surface, or NULL if there was an error.
 *
 * @public @memberof android_custom_surface
 */
struct android_custom_surface *
android_custom_surface_async_start(struct _JavaVM *vm,
                                   void *context,
                                   int32_t display_id,
                                   const char *surface_title,
                                   int32_t preferred_display_mode_id);

/*!
 * Destroy the native handle for the custom surface.
 *
 * Depending on the state, this may not necessarily destroy the underlying
 * surface, if other references exist. However, a flag will be set to indicate
 * that native code is done using it.
 *
 * @param ptr_custom_surface Pointer to the opaque pointer: will be set to NULL.
 *
 * @public @memberof android_custom_surface
 */
void
android_custom_surface_destroy(struct android_custom_surface **ptr_custom_surface);

/*!
 * Get the ANativeWindow pointer corresponding to the added Surface, if
 * available, waiting up to the specified duration.
 *
 * This may return NULL because the underlying operation is asynchronous.
 *
 * @public @memberof android_custom_surface
 */
ANativeWindow *
android_custom_surface_wait_get_surface(struct android_custom_surface *custom_surface, uint64_t timeout_ms);

bool
android_custom_surface_get_display_metrics(struct _JavaVM *vm,
                                           void *activity,
                                           struct xrt_android_display_metrics *out_metrics);

bool
android_custom_surface_can_draw_overlays(struct _JavaVM *vm, void *context);

float
android_custom_surface_get_display_refresh_rate(struct _JavaVM *vm, void *context);

#ifdef __cplusplus
}
#endif

#endif // XRT_OS_ANDROID

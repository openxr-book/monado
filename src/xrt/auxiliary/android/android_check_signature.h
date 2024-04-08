// Copyright 2024, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Functions for Android app signature check.
 * @author Zhisheng Lv
 * @ingroup aux_android
 */

#pragma once

#include <xrt/xrt_config_os.h>

#ifdef XRT_OS_ANDROID

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Check if the application has the same signture with runtime or not.
 */
bool
android_check_signature(void *application_context, const char *runtime_package_name);

/*!
 * Check if the target extension name is enabled.
 */
bool
is_extension_enabled(const unsigned int enabled_extension_count,
                     const char *const *enabled_extension_names,
                     const char *target_extension_name);

/*!
 * Check if property "debug.openxr.runtime.checkOverlaySignature" values "true".
 */
bool
is_check_overlay_signature_property_enabled();

#ifdef __cplusplus
}
#endif

#endif // XRT_OS_ANDROID
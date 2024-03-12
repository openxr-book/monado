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

#ifdef __cplusplus
}
#endif

#endif // XRT_OS_ANDROID
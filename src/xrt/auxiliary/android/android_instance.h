// Copyright 2023, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Implementation of android instance functions.
 * @author Jarvis Huang
 * @ingroup aux_android
 */

#pragma once

#include <xrt/xrt_config_os.h>
#include <xrt/xrt_android.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XRT_OS_ANDROID

xrt_result_t
xrt_instance_android_create(struct xrt_instance_info *ii, struct xrt_instance_android **out_inst_android);

#endif // XRT_OS_ANDROID

#ifdef __cplusplus
}
#endif

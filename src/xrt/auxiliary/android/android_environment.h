// Copyright 2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Environment class function.
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup aux_android
 */
#pragma once

#include <xrt/xrt_config_os.h>

#ifdef XRT_OS_ANDROID
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool
android_enviroment_get_external_storage_dir(char *str, size_t size);


#ifdef __cplusplus
}
#endif

#endif
// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to sample driver.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup drv_sample
 */

#pragma once

#include "xrt/xrt_frameserver.h"

#ifdef __cplusplus
extern "C" {
#endif

// Stereolabs ZED Mini
#define SLZM_VID 0x2B03
#define SLZM_PID 0xF681

struct xrt_auto_prober *
sl_create_auto_prober(void);

struct xrt_device *
sl_zed_mini_create(void);

struct xrt_fs *
sl_frameserver_create(struct xrt_frame_context *xfctx);

#ifdef __cplusplus
}
#endif

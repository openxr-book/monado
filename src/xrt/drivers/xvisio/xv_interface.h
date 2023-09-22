// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Interface to xvisio driver.
 * @author Joseph Albers <joseph.albers@outlook.de>
 * @ingroup drv_xvisio
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// // Stereolabs ZED Mini
// #define SLZM_VID 0x2B03
// #define SLZM_PID 0xF681

struct xrt_auto_prober *
xvisio_create_auto_prober(void);

struct xrt_device *
xvisio_xr50_create(void);

// struct xrt_fs *
// sl_frameserver_create(struct xrt_frame_context *xfctx);

#ifdef __cplusplus
}
#endif
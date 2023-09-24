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

// Xvisio SeerSense XR50
#define XR50_VID 0x040E
#define XR50_PID 0xF408

struct xrt_auto_prober *
xvisio_create_auto_prober(void);

struct xrt_device *
xvisio_xr50_create(void);

// struct xrt_fs *
// sl_frameserver_create(struct xrt_frame_context *xfctx);

#ifdef __cplusplus
}
#endif
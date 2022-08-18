// Copyright 2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  SVR WIP
 * @author Moses Turner <moses@collabora.com>
 * @ingroup drv_ns
 */

#pragma once

#include "math/m_api.h"
#include "util/u_json.h"
#include "util/u_misc.h"
#include "xrt/xrt_defines.h"
#include "xrt/xrt_device.h"
#include "util/u_logging.h"
#include "os/os_threading.h"
#include "util/u_distortion_mesh.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *
 * Printing functions.
 *
 */

#define SVR_TRACE(d, ...) U_LOG_XDEV_IFL_T(&d->base, d->log_level, __VA_ARGS__)
#define SVR_DEBUG(d, ...) U_LOG_XDEV_IFL_D(&d->base, d->log_level, __VA_ARGS__)
#define SVR_INFO(d, ...) U_LOG_XDEV_IFL_I(&d->base, d->log_level, __VA_ARGS__)
#define SVR_WARN(d, ...) U_LOG_XDEV_IFL_W(&d->base, d->log_level, __VA_ARGS__)
#define SVR_ERROR(d, ...) U_LOG_XDEV_IFL_E(&d->base, d->log_level, __VA_ARGS__)

#define HALF_FOV 0.9f

struct svr_hmd
{
	struct xrt_device base;

	enum u_logging_level log_level;
};

static inline struct svr_hmd *
svr_hmd(struct xrt_device *xdev)
{
	return (struct svr_hmd *)xdev;
}

#ifdef __cplusplus
}
#endif

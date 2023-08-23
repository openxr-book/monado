// Copyright 2023, Joseph Albers.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief daemon HMD code.
 * @author Joseph Albers <joseph.albers@outlook.de>
 * @ingroup drv_daemon
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

/*!
 * Information about the whole daemon headset.
 *
 * @ingroup drv_daemon
 * @implements xrt_device
 */

struct daemon_hmd
{
	struct xrt_device base;
	struct xrt_space_relation tracker_relation;
	const cJSON *config_json;

	enum u_logging_level log_level;
};

/*
 *
 * Functions
 *
 */

/*!
 * Get the dameon headset information from a @ref xrt_device.
 *
 * @ingroup drv_daemon
 */
static inline struct daemon_hmd *
daemon_hmd(struct xrt_device *xdev)
{
	return (struct daemon_hmd *)xdev;
}

#ifdef __cplusplus
}
#endif

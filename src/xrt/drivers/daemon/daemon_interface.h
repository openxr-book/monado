// Copyright 2023, Joseph Albers.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Interface to daemon driver code.
 * @author Joseph Albers <joseph.albers@outlook.de>
 * @ingroup drv_daemon
 */

#pragma once
#include "util/u_json.h"
#include "xrt/xrt_device.h"

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup drv_daemon daemon driver
 * @ingroup drv
 *
 * @brief Driver for the daemon HMD.
 */

/*!
 * Creates a daemon HMD.
 *
 * @ingroup drv_daemon
 */

struct xrt_device *
daemon_hmd_create(const cJSON *config_json);

/*!
 * @dir drivers/daemon
 *
 * @brief @ref drv_daemon files.
 */


#ifdef __cplusplus
}
#endif

// Copyright 2021, Mateo de Mayo.
// Copyright 2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Interface to @ref drv_qwerty.
 * @author Mateo de Mayo <mateodemayo@gmail.com>
 * @ingroup drv_qwerty
 */

#pragma once

#include "xrt/xrt_compiler.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef union SDL_Event SDL_Event;

/*!
 * @defgroup drv_qwerty Qwerty driver
 * @ingroup drv
 *
 * @brief Driver for emulated HMD and controllers through keyboard and mouse.
 */

/*!
 * @dir drivers/qwerty
 *
 * @brief @ref drv_qwerty files.
 */

/*!
 * Create an auto prober for qwerty devices.
 *
 * @ingroup drv_qwerty
 */
struct xrt_auto_prober *
qwerty_create_auto_prober(void);

/*!
 * Process an SDL_Event (like a key press) and dispatches a suitable action
 * to the appropriate qwerty_device.
 *
 * @note A qwerty_controller might not be in use (for example if you have
 * physical controllers connected), though its memory will be modified by these
 * events regardless. A qwerty_hmd not in use will not be modified as it never
 * gets created.
 *
 * @ingroup drv_qwerty
 */
void
qwerty_process_event(struct xrt_device **xdevs, size_t xdev_count, SDL_Event event);


#ifdef __cplusplus
}
#endif

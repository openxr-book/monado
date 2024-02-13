// Copyright 2024, Gavin John
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Driver for Xreal Light glasses.
 * @author Gavin John <gavinnjohn@gmail.com>
 * @ingroup drv_xreal_light
 */

#pragma once

#include <stddef.h>
#include "os/os_hid.h"
#include "xrt/xrt_compiler.h"
#include "util/u_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @defgroup drv_xreal_light Xreal Light driver
 * @ingroup drv
 *
 * @brief Monado driver for the Xreal Light glasses.
 */

/*!
 * Vendor id for Xreal Light Microcontroller
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_MCU_VID 0x0486

/*!
 * Product id for Xreal Light Microcontroller
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_MCU_PID 0x573c

/*!
 * HID interface number for Xreal Light Microcontroller
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_MCU_IFACE 0

/*!
 * Vendor id for Xreal Light OV580 DSP
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_OV580_VID 0x05a9

/*!
 * Product id for Xreal Light OV580 DSP
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_OV580_PID 0x0680

/*!
 * HID interface number for Xreal Light OV580 DSP
 * (The IMU is attached to the DSP over HID instead
 * of the MCU for some odd reason)
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_OV580_IFACE 2

/*!
 * ASCII-encoded manufacturer ID for Xreal Light EDID data
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_EDID_MANUFACTURER_ID "MRG"

/*!
 * Numeric product ID for Xreal Light EDID data
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_EDID_PRODUCT_ID 0x3131

/*!
 * Display modes for Xreal Light glasses.
 * 
 * SAME_ON_BOTH: Both eyes see the same image.
 * HALF_SBS: Side-by-side stereo image, half refresh rate.
 * STEREO: Full stereo image.
 * HIGH_REFRESH_RATE_SBS: Side-by-side stereo image, full refresh rate.
 *
 * @ingroup drv_xreal_light
 */
#define XREAL_LIGHT_DISPLAY_MODE_SAME_ON_BOTH 0x31
#define XREAL_LIGHT_DISPLAY_MODE_HALF_SBS 0x32
#define XREAL_LIGHT_DISPLAY_MODE_STEREO 0x33
#define XREAL_LIGHT_DISPLAY_MODE_HIGH_REFRESH_RATE_SBS 0x34

// TODO: Document this
#define XREAL_LIGHT_HEARTBEAT_INTERVAL_MS 250
#define XREAL_LIGHT_MCU_DATA_BUFFER_SIZE 64
#define XREAL_LIGHT_MCU_CONTROL_BUFFER_SIZE 64
#define XREAL_LIGHT_OV580_DATA_BUFFER_SIZE 64
#define XREAL_LIGHT_OV580_CONTROL_BUFFER_SIZE 64

/*!
 * Builder setup for Xreal Light glasses.
 *
 * @ingroup drv_xreal_light
 */
struct xrt_device *
xreal_light_hmd_create_device(struct os_hid_device *mcu_hid_handle,
                              struct os_hid_device *ov580_hid_handle);

/*!
 * @dir drivers/xreal_light
 *
 * @brief xreal_light files.
 */

#ifdef __cplusplus
}
#endif

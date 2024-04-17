// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to nolo driver.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup drv_nolo
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "util/u_logging.h"
#include "xrt/xrt_prober.h"
#include "xrt/xrt_defines.h"
#include "math/m_imu_3dof.h"
#include "xrt/xrt_device.h"
#include "util/u_device.h"
#include "nolo_interface.h"

//DEBUG_GET_ONCE_LOG_OPTION(nolo_log, "NOLO_LOG", U_LOGGING_WARN)

//#define DEBUG_INPUT 
//#define DEBUG_POSITION
//#define DEBUG_PLOT
//#define DEBUG_ROTATION
//#define DEBUG_USB_TRACKER_PACKET
//#define DEBUG_USB_CONTROLLER_PACKET

#ifdef DEBUG_INPUT
	#define NOLO_DEBUG_INPUT(p, ...) U_LOG_XDEV_IFL_D(&device->base, device->log_level, __VA_ARGS__)
#else
    #define NOLO_DEBUG_INPUT(p, ...)
#endif

#ifdef DEBUG_POSITION
	#define NOLO_DEBUG_POSITION(p, ...) U_LOG_XDEV_IFL_D(&device->base, device->log_level, __VA_ARGS__)
#else
	#define NOLO_DEBUG_POSITION(p, ...)
#endif

#ifdef DEBUG_ROTATION
	#define NOLO_DEBUG_ROTATION(p, ...) U_LOG_XDEV_IFL_D(&device->base, device->log_level, __VA_ARGS__)
#else
	#define NOLO_DEBUG_ROTATION(p, ...)
#endif

#ifdef DEBUG_USB_TRACKER_PACKET
	#define NOLO_DEBUG_USB_TRACKER_PACKET(p, ...) U_LOG_XDEV_IFL_D(&device->base, device->log_level, __VA_ARGS__)
#else
	#define NOLO_DEBUG_USB_TRACKER_PACKET(p, ...)
#endif

#ifdef DEBUG_USB_CONTROLLER_PACKET
	#define NOLO_DEBUG_USB_CONTROLLER_PACKET(p, ...) U_LOG_XDEV_IFL_D(&device->base, device->log_level, __VA_ARGS__)
#else
	#define NOLO_DEBUG_USB_CONTROLLER_PACKET(p, ...)
#endif

#ifdef DEBUG_PLOT
	#define NOLO_DEBUG_PLOT(...) U_LOG_RAW(__VA_ARGS__)
#else
	#define NOLO_DEBUG_PLOT(...)
#endif

#define PLOT_HMD 0
#define PLOT_CONTROLLER1 0
#define PLOT_CONTROLLER2 0

#define NOLO_TRACE(p, ...) U_LOG_XDEV_IFL_T(&device->base, device->log_level, __VA_ARGS__)
#define NOLO_DEBUG(p, ...) U_LOG_XDEV_IFL_D(&device->base, device->log_level, __VA_ARGS__)
#define NOLO_ERROR(p, ...) U_LOG_XDEV_IFL_E(&device->base, device->log_level, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif


void print_device_info(struct xrt_prober_device *dev, unsigned char product_name[128]);
void print_digital_input(struct nolo_device * device, int index);
void print_analog_input(struct nolo_device * device, int index_x, int index_y);
void print_controller_inputs(struct nolo_device *device);
void print_plot_data(struct nolo_device *device);
void print_nolo_tracker_position(struct nolo_device *device);
void print_nolo_tracker_rotation(struct nolo_device *device);
void print_nolo_tracker_rotation_full(struct nolo_device *device);
void print_nolo_controller_trigger_pulled(struct nolo_device *device);

#ifdef __cplusplus
}
#endif

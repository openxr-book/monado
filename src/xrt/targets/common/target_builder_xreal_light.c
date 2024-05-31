// Copyright 2024, Gavin John
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Driver for Xreal Light glasses.
 * @author Gavin John <gavinnjohn@gmail.com>
 * @ingroup drv_xreal_light
 */

#include "xrt/xrt_prober.h"

#include "os/os_hid.h"

#include "util/u_misc.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_builders.h"
#include "util/u_config_json.h"
#include "util/u_system_helpers.h"

#include "xreal_light/xreal_light_interface.h"

#ifdef XRT_OS_LINUX
#include "util/u_linux.h"
#endif

#include <libusb.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


enum u_logging_level xreal_light_log_level;

#define XREAL_LIGHT_DEBUG(...) U_LOG_IFL_D(xreal_light_log_level, __VA_ARGS__)
#define XREAL_LIGHT_TRACE(...) U_LOG_IFL_T(xreal_light_log_level, __VA_ARGS__)
#define XREAL_LIGHT_WARN(...) U_LOG_IFL_W(xreal_light_log_level, __VA_ARGS__)
#define XREAL_LIGHT_ERROR(...) U_LOG_IFL_E(xreal_light_log_level, __VA_ARGS__)

DEBUG_GET_ONCE_LOG_OPTION(xreal_light_log, "XREAL_LIGHT_LOG", U_LOGGING_DEBUG)

static const char *driver_list[] = {
    "xreal_light",
};

static xrt_result_t
xreal_light_estimate_system(struct xrt_builder *xb,
                            cJSON *config,
                            struct xrt_prober *xp,
                            struct xrt_builder_estimate *estimate)
{
	// Reset the estimate (default: device not found)
	U_ZERO(estimate);

	// Set variables to reuse
	xrt_result_t xret = XRT_SUCCESS;
	struct xrt_prober_device **xpdevs = NULL;
	size_t xpdev_count = 0;

	xret = xrt_prober_lock_list(xp, &xpdevs, &xpdev_count);
	if (xret != XRT_SUCCESS) {
		XREAL_LIGHT_ERROR("Failed to lock the prober list for system estimation.");
		XREAL_LIGHT_DEBUG("xrt_prober_lock_list failed with code %d", xret);
		return xret;
	}

	if (u_builder_find_prober_device(xpdevs, xpdev_count, XREAL_LIGHT_MCU_VID,
	                                       XREAL_LIGHT_MCU_PID, XRT_BUS_TYPE_USB) == NULL) {
		XREAL_LIGHT_DEBUG("Did not find the MCU device.");
		XREAL_LIGHT_DEBUG("Xreal Light glasses not detected.");
		if ((xret = xrt_prober_unlock_list(xp, &xpdevs)) != XRT_SUCCESS) {
			XREAL_LIGHT_ERROR("Failed to unlock the prober list in MCU detection.");
			XREAL_LIGHT_DEBUG("xrt_prober_unlock_list failed with code %d", xret);
			return xret;
		}
		return XRT_SUCCESS;
	} else {
		XREAL_LIGHT_DEBUG("Found the MCU device.");
	}

	if (u_builder_find_prober_device(xpdevs, xpdev_count, XREAL_LIGHT_OV580_VID,
	                                       XREAL_LIGHT_OV580_PID, XRT_BUS_TYPE_USB) == NULL) {
		XREAL_LIGHT_WARN("Found MCU device but did not find OV580 device.");
		XREAL_LIGHT_WARN("This is not expected and is probably a bug.");
		XREAL_LIGHT_WARN("Please report this to Monado developers.");
		XREAL_LIGHT_DEBUG("Did not find the OV580 device.");
		XREAL_LIGHT_DEBUG("Xreal Light glasses not detected.");
		if ((xret = xrt_prober_unlock_list(xp, &xpdevs)) != XRT_SUCCESS) {
			XREAL_LIGHT_ERROR("Failed to unlock the prober list in MCU detection.");
			XREAL_LIGHT_DEBUG("xrt_prober_unlock_list failed with code %d", xret);
			return xret;
		}
		return XRT_SUCCESS;
	} else {
		XREAL_LIGHT_DEBUG("Found the OV580 device.");
	}

	XREAL_LIGHT_DEBUG("Found both MCU and OV580 devices.");
	XREAL_LIGHT_DEBUG("Xreal Light glasses detected.");

	if ((xret = xrt_prober_unlock_list(xp, &xpdevs)) != XRT_SUCCESS) {
		XREAL_LIGHT_ERROR("Failed to unlock the prober list in system estimation.");
		XREAL_LIGHT_DEBUG("xrt_prober_unlock_list failed with code %d", xret);
		return xret;
	}

	estimate->certain.head = true; // Indicates we are certain that the device is present.
	return XRT_SUCCESS;
}

static xrt_result_t
xreal_light_open_system_impl(struct xrt_builder *xb,
                             cJSON *config,
                             struct xrt_prober *xp,
                             struct xrt_tracking_origin *origin,
                             struct xrt_system_devices *xsysd,
                             struct xrt_frame_context *xfctx,
                             struct u_builder_roles_helper *ubrh)
{
	xreal_light_log_level = debug_get_log_option_xreal_light_log();

	xrt_result_t xret;
	struct xrt_prober_device **xpdevs = NULL;
	size_t xpdev_count = 0;

	xret = xrt_prober_lock_list(xp, &xpdevs, &xpdev_count);
	if (xret != XRT_SUCCESS) {
		XREAL_LIGHT_ERROR("Failed to lock the prober list for system opening.");
		XREAL_LIGHT_DEBUG("xrt_prober_lock_list failed with code %d", xret);
		goto fail;
	}

	struct xrt_prober_device *devMCU = u_builder_find_prober_device(xpdevs, xpdev_count, XREAL_LIGHT_MCU_VID,
	                                                                XREAL_LIGHT_MCU_PID, XRT_BUS_TYPE_USB);
	struct xrt_prober_device *devOV580 = u_builder_find_prober_device(xpdevs, xpdev_count, XREAL_LIGHT_OV580_VID,
	                                                                  XREAL_LIGHT_OV580_PID, XRT_BUS_TYPE_USB);
	
	if (devMCU == NULL) {
		XREAL_LIGHT_ERROR("Failed to find the MCU device.");
		goto unlock_and_fail;
	}
	if (devOV580 == NULL) {
		XREAL_LIGHT_ERROR("Failed to find the OV580 device.");
		goto unlock_and_fail;
	}
	
	struct os_hid_device *mcu_hid_handle = NULL;
	int result_mcu = xrt_prober_open_hid_interface(xp, devMCU, XREAL_LIGHT_MCU_IFACE, &mcu_hid_handle);
	if (result_mcu != 0) {
		XREAL_LIGHT_ERROR("Failed to open MCU HID interface.");
		XREAL_LIGHT_DEBUG("xrt_prober_open_hid_interface failed with code %d", result_mcu);
		goto unlock_and_fail;
	}

	struct os_hid_device *ov580_hid_handle = NULL;
	int result_ov580 = xrt_prober_open_hid_interface(xp, devOV580, XREAL_LIGHT_OV580_IFACE, &ov580_hid_handle);
	if (result_ov580 != 0) {
		XREAL_LIGHT_ERROR("Failed to open OV580 HID interface.");
		XREAL_LIGHT_DEBUG("xrt_prober_open_hid_interface failed with code %d", result_ov580);
		goto unlock_and_fail;
	}

	if ((xret = xrt_prober_unlock_list(xp, &xpdevs)) != XRT_SUCCESS) {
		XREAL_LIGHT_ERROR("Failed to unlock the prober list in system opening.");
		XREAL_LIGHT_DEBUG("xrt_prober_unlock_list failed with code %d", xret);
		goto fail;
	}

	struct xrt_device *xreal_light_device =
	    xreal_light_hmd_create_device(mcu_hid_handle, ov580_hid_handle);
	
	if (xreal_light_device == NULL) {
		XREAL_LIGHT_ERROR("Failed to create Xreal Light glasses device.");
		goto fail;
	}

	xsysd->xdevs[xsysd->xdev_count++] = xreal_light_device;

	ubrh->head = xreal_light_device;

	return XRT_SUCCESS;
unlock_and_fail:
	if (xpdevs != NULL) {
		xrt_prober_unlock_list(xp, &xpdevs);
	}
	goto fail;
fail:
	return XRT_ERROR_DEVICE_CREATION_FAILED;
}

static void
xreal_light_destroy(struct xrt_builder *xb)
{
	free(xb);
}

struct xrt_builder *
t_builder_xreal_light_create(void)
{
	struct u_builder *ub = U_TYPED_CALLOC(struct u_builder);

	// xrt_builder fields.
	ub->base.estimate_system = xreal_light_estimate_system;
	ub->base.open_system = u_builder_open_system_static_roles;
	ub->base.destroy = xreal_light_destroy;
	ub->base.identifier = "xreal_light";
	ub->base.name = "Xreal Light glasses builder";
	ub->base.driver_identifiers = driver_list;
	ub->base.driver_identifier_count = ARRAY_SIZE(driver_list);

	// u_builder fields.
	ub->open_system_static_roles = xreal_light_open_system_impl;

	return &ub->base;
}

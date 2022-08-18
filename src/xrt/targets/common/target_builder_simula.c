// Copyright 2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Builder for Lighthouse-tracked devices (vive, index, tundra trackers, etc.)
 * @author Moses Turner <moses@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup xrt_iface
 */

#include "multi_wrapper/multi.h"
#include "realsense/rs_interface.h"
#include "tracking/t_hand_tracking.h"
#include "tracking/t_tracking.h"

#include "xrt/xrt_config_drivers.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_prober.h"

#include "util/u_builders.h"
#include "util/u_config_json.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_sink.h"
#include "util/u_system_helpers.h"

#include "target_builder_interface.h"

#include "simula/svr_interface.h"
#include "v4l2/v4l2_interface.h"

#include "xrt/xrt_frameserver.h"
#include "xrt/xrt_results.h"
#include "xrt/xrt_tracking.h"

#include <assert.h>

DEBUG_GET_ONCE_BOOL_OPTION(simula_enable, "SIMULA_ENABLED", false)
DEBUG_GET_ONCE_LOG_OPTION(svr_log, "SIMULA_LOG", U_LOGGING_WARN)


#define SVR_TRACE(...) U_LOG_IFL_T(debug_get_log_option_svr_log(), __VA_ARGS__)
#define SVR_DEBUG(...) U_LOG_IFL_D(debug_get_log_option_svr_log(), __VA_ARGS__)
#define SVR_INFO(...) U_LOG_IFL_I(debug_get_log_option_svr_log(), __VA_ARGS__)
#define SVR_WARN(...) U_LOG_IFL_W(debug_get_log_option_svr_log(), __VA_ARGS__)
#define SVR_ERROR(...) U_LOG_IFL_E(debug_get_log_option_svr_log(), __VA_ARGS__)

static const char *driver_list[] = {
    "simula",
};

#define MOVIDIUS_VID 0x03E7
#define MOVIDIUS_PID 0x2150

#define TM2_VID 0x8087
#define TM2_PID 0x0B37


static xrt_result_t
svr_estimate_system(struct xrt_builder *xb, cJSON *config, struct xrt_prober *xp, struct xrt_builder_estimate *estimate)
{
	U_ZERO(estimate);

	if (!debug_get_bool_option_simula_enable()) {
		// No failure occurred - the user just didn't ask for Simula
		return XRT_SUCCESS;
	}

	struct xrt_prober_device **xpdevs = NULL;
	size_t xpdev_count = 0;
	xrt_result_t xret = XRT_SUCCESS;

	// Lock the device list
	xret = xrt_prober_lock_list(xp, &xpdevs, &xpdev_count);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	bool movidius = u_builder_find_prober_device(xpdevs, xpdev_count, MOVIDIUS_VID, MOVIDIUS_PID, XRT_BUS_TYPE_USB);
	bool tm2 = u_builder_find_prober_device(xpdevs, xpdev_count, TM2_VID, TM2_PID, XRT_BUS_TYPE_USB);

	if (!movidius && !tm2) {
		U_LOG_E("Simula enabled but couldn't find realsense device!");
		return XRT_SUCCESS;
	}

	// I think that ideally we want `movidius` - in that case I think when we grab the device, it reboots to `tm2`


	estimate->maybe.head = true;
	estimate->certain.head = true;


	return XRT_SUCCESS;
}

static xrt_result_t
svr_open_system(struct xrt_builder *xb, cJSON *config, struct xrt_prober *xp, struct xrt_system_devices **out_xsysd)
{
	struct u_system_devices *usysd = u_system_devices_allocate();
	xrt_result_t result = XRT_SUCCESS;

	if (out_xsysd == NULL || *out_xsysd != NULL) {
		SVR_ERROR("Invalid output system pointer");
		result = XRT_ERROR_DEVICE_CREATION_FAILED;
		goto end;
	}


	// The below is a garbage hack - we should remove the autoprober entirely - but I'm tired af and need to ship it

	struct xrt_device *t265_dev = create_tracked_rs_device(xp);

	struct xrt_device *svr_dev = svr_hmd_create();

	struct xrt_pose ident = XRT_POSE_IDENTITY;


	struct xrt_device *head_device = multi_create_tracking_override(
	    XRT_TRACKING_OVERRIDE_ATTACHED, svr_dev, t265_dev, XRT_INPUT_GENERIC_TRACKER_POSE, &ident);

	usysd->base.roles.head = head_device;
	usysd->base.xdevs[0] = usysd->base.roles.head;
	usysd->base.xdev_count = 1;


end:
	if (result == XRT_SUCCESS) {
		*out_xsysd = &usysd->base;
	} else {
		u_system_devices_destroy(&usysd);
	}

	return result;
}

static void
svr_destroy(struct xrt_builder *xb)
{
	free(xb);
}

/*
 *
 * 'Exported' functions.
 *
 */

struct xrt_builder *
t_builder_simula_create(void)
{
	struct xrt_builder *xb = U_TYPED_CALLOC(struct xrt_builder);
	xb->estimate_system = svr_estimate_system;
	xb->open_system = svr_open_system;
	xb->destroy = svr_destroy;
	xb->identifier = "simula";
	xb->name = "SimulaVR headset";
	xb->driver_identifiers = driver_list;
	xb->driver_identifier_count = ARRAY_SIZE(driver_list);

	return xb;
}

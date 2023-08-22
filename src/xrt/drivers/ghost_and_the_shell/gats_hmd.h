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
 * Information about the whole North Star headset.
 *
 * @ingroup drv_ns
 * @implements xrt_device
 */

struct gats_hmd
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
 * Get the Ghost and the Shell information from a @ref xrt_device.
 *
 * @ingroup drv_gats
 */
static inline struct gats_hmd *
gats_hmd(struct xrt_device *xdev)
{
	return (struct gats_hmd *)xdev;
}

#ifdef __cplusplus
}
#endif

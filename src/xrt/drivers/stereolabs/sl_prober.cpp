// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Sample prober code.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup drv_sample
 */

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "sample_interface.h"


/*!
 * @implements xrt_auto_prober
 */
struct sl_auto_prober
{
	struct xrt_auto_prober base;
};

//! @private @memberof sl_auto_prober
static inline struct sl_auto_prober *
sl_auto_prober(struct xrt_auto_prober *p)
{
	return (struct sl_auto_prober *)p;
}

//! @private @memberof sl_auto_prober
static void
sl_auto_prober_destroy(struct xrt_auto_prober *p)
{
	struct sl_auto_prober *sl_ap = sl_auto_prober(p);

	free(sl_ap);
}

//! @public @memberof sl_auto_prober
static int
sl_auto_prober_autoprobe(struct xrt_auto_prober *xap,
                             cJSON *attached_data,
                             bool no_hmds,
                             struct xrt_prober *xp,
                             struct xrt_device **out_xdevs)
{
	struct sl_auto_prober *sl_ap = sl_auto_prober(xap);
	(void)sl_ap;

	out_xdevs[0] = sl_zed_mini_create();
	return 1;
}

struct xrt_auto_prober *
sl_create_auto_prober()
{
	struct sl_auto_prober *sap = U_TYPED_CALLOC(struct sl_auto_prober);
	sap->base.name = "Stereolabs";
	sap->base.destroy = sl_auto_prober_destroy;
	sap->base.lelo_dallas_autoprobe = sl_auto_prober_autoprobe;

	return &sap->base;
}

// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief XVisio prober code.
 * @author Joseph Albers <joseph.albers@outlook.de>
 * @ingroup drv_xvisio
 */

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "xv_interface.h"

#include <stdio.h>

struct xvisio_auto_prober
{
	struct xrt_auto_prober base;
};

static inline struct xvisio_auto_prober *
xv_auto_prober(struct xrt_auto_prober *p)
{
	return (struct xvisio_auto_prober *)p;
}

static void
xvisio_auto_prober_destroy(struct xrt_auto_prober *p)
{
	struct xvisio_auto_prober *xvisio_auto_prober = xv_auto_prober(p);
	free(xvisio_auto_prober);
}

static int
xvisio_auto_prober_autoprobe(struct xrt_auto_prober *xap,
                         cJSON *attached_data,
                         bool no_hmds,
                         struct xrt_prober *xp,
                         struct xrt_device **out_xdevs)
{
	struct xvisio_auto_prober *xvisio_auto_prober = xv_auto_prober(xap);
	(void)xvisio_auto_prober;

	out_xdevs[0] = xvisio_xr50_create();
	return 1;
}

struct xrt_auto_prober *
xvisio_create_auto_prober()
{
	struct xvisio_auto_prober *xvisio_auto_prober = U_TYPED_CALLOC(struct xvisio_auto_prober);
	xvisio_auto_prober->base.name = "xvisio_auto_prober";
	xvisio_auto_prober->base.destroy = xvisio_auto_prober_destroy;
	xvisio_auto_prober->base.lelo_dallas_autoprobe = xvisio_auto_prober_autoprobe;

	return &xvisio_auto_prober->base;
}
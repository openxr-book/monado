// Copyright 2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Basic xrt_instance_base implementation.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 */

#include "android_instance_base.h"

#include "xrt/xrt_instance.h"
#include "xrt/xrt_android.h"


static inline struct android_instance_base *
android_instance_base(struct xrt_instance_android *xib)
{
	return (struct android_instance_base *)(xib);
}

static inline struct android_instance_base *
android_instance_base_get(struct xrt_instance *xinst)
{
	return android_instance_base(xinst->android_instance);
}

static int
base_store_vm(struct xrt_instance *xinst, struct _JavaVM *vm)
{
	struct android_instance_base *aib = android_instance_base_get(xinst);
	aib->vm = vm;
	return 0;
}

static struct _JavaVM *
base_get_vm(struct xrt_instance *xinst)
{

	struct android_instance_base *aib = android_instance_base_get(xinst);
	return aib->vm;
}
static int
base_store_context(struct xrt_instance *xinst, void *context)
{
	struct android_instance_base *aib = android_instance_base_get(xinst);
	aib->context = context;
	return 0;
}

static void *
base_get_context(struct xrt_instance *xinst)
{

	struct android_instance_base *aib = android_instance_base_get(xinst);
	return aib->context;
}

static int
base_register_surface_callback(struct xrt_instance *xinst,
                               xrt_android_surface_event_handler_t callback,
                               enum xrt_android_surface_event event_mask,
                               void *userdata)
{
	struct android_instance_base *aib = android_instance_base_get(xinst);
	return android_surface_callbacks_register_callback(aib->surface_callbacks, callback, event_mask, userdata);
}

static int
base_remove_surface_callback(struct xrt_instance *xinst,
                             xrt_android_surface_event_handler_t callback,
                             enum xrt_android_surface_event event_mask,
                             void *userdata)
{
	struct android_instance_base *aib = android_instance_base_get(xinst);
	return android_surface_callbacks_remove_callback(aib->surface_callbacks, callback, event_mask, userdata);
}

int
android_instance_base_init(struct android_instance_base *aib,
                           struct xrt_instance *xinst,
                           struct _JavaVM *vm,
                           void *activity)
{
	aib->surface_callbacks = android_surface_callbacks_create(xinst);
	if (aib->surface_callbacks == NULL) {
		return -1;
	}
	aib->vm = vm;
	aib->context = activity;
	aib->base.store_vm = base_store_vm;
	aib->base.get_vm = base_get_vm;
	aib->base.store_context = base_store_context;
	aib->base.get_context = base_get_context;
	aib->base.register_surface_callback = base_register_surface_callback;
	aib->base.remove_surface_callback = base_remove_surface_callback;
	return 0;
}

void
android_instance_base_cleanup(struct android_instance_base *aib)
{
	android_surface_callbacks_destroy(&aib->surface_callbacks);
}

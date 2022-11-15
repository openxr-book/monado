// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Base implementation of the @ref xrt_instance_android interface.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 */
#pragma once

#include "xrt/xrt_instance.h"
#include "xrt/xrt_android.h"
#include "android_surface_callbacks.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _JavaVM;

/*
 *
 * Struct and helpers.
 *
 */

/*!
 * @brief Base implementation of the @ref xrt_instance_android interface.
 *
 */
struct android_instance_base
{
	struct xrt_instance_android base;
	struct _JavaVM *vm;
	void *context;
	struct android_surface_callbacks *surface_callbacks;
};

/*!
 * @brief Initialize resources owned by @p android_instance_base
 *
 * @param aib The object to initialize.
 * @param xinst The xrt_instance to store in the surface callbacks object.
 * @param vm The JavaVM pointer.
 * @param activity The activity jobject, cast to a void pointer.
 *
 * @public @memberof android_instance_base
 */
int
android_instance_base_init(struct android_instance_base *aib, struct xrt_instance *xinst, struct _JavaVM *vm, void *activity);

/*!
 * @brief Release resources owned by @p android_instance_base - but does not free @p aib itself!
 *
 * @public @memberof android_instance_base
 */
void
android_instance_base_cleanup(struct android_instance_base *aib);

#ifdef __cplusplus
};
#endif

// Copyright 2023, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Implementation of xrt_instance_android interface.
 * @author Jarvis Huang
 * @ingroup aux_android
 */

#include "android_instance.h"

#include "android_lifecycle_callbacks.h"

#include "util/u_logging.h"
#include "util/u_misc.h"
#include "xrt/xrt_instance.h"

#include <jni.h>

struct android_instance
{
	struct xrt_instance_android base;
	struct _JavaVM *vm;
	void *context;
	struct android_lifecycle_callbacks *lifecycle_callbacks;
};

static struct android_instance *
android_instance(struct xrt_instance_android *xinst_android)
{
	return (struct android_instance *)xinst_android;
}

static struct _JavaVM *
android_instance_get_vm(struct xrt_instance_android *xinst_android)
{
	struct android_instance *inst = android_instance(xinst_android);
	return inst->vm;
}

static void *
android_instance_get_context(struct xrt_instance_android *xinst_android)
{
	struct android_instance *inst = android_instance(xinst_android);
	return inst->context;
}

static xrt_result_t
android_instance_register_activity_lifecycle_callback(struct xrt_instance_android *xinst_android,
                                                      xrt_android_lifecycle_event_handler_t callback,
                                                      enum xrt_android_lifecycle_event event_mask,
                                                      void *userdata)
{
	struct android_instance *inst = android_instance(xinst_android);
	int ret = -1;
	if (inst->lifecycle_callbacks != NULL) {
		ret = android_lifecycle_callbacks_register_callback(inst->lifecycle_callbacks, callback, event_mask,
		                                                    userdata);
	}
	return ret == 0 ? XRT_SUCCESS : XRT_ERROR_ANDROID;
}

static xrt_result_t
android_instance_remove_activity_lifecycle_callback(struct xrt_instance_android *xinst_android,
                                                    xrt_android_lifecycle_event_handler_t callback,
                                                    enum xrt_android_lifecycle_event event_mask,
                                                    void *userdata)
{
	struct android_instance *inst = android_instance(xinst_android);
	int ret = 0;
	if (inst->lifecycle_callbacks != NULL) {
		ret = android_lifecycle_callbacks_remove_callback(inst->lifecycle_callbacks, callback, event_mask,
		                                                  userdata);
	}
	return ret > 0 ? XRT_SUCCESS : XRT_ERROR_ANDROID;
}

static void
android_instance_destroy(struct xrt_instance_android *xinst_android)
{
	struct android_instance *inst = android_instance(xinst_android);

	android_lifecycle_callbacks_destroy(&inst->lifecycle_callbacks);

	if (inst->vm != NULL) {
		JNIEnv *env = NULL;
		if (inst->vm->functions->AttachCurrentThread(&inst->vm->functions, &env, NULL) == JNI_OK) {
			(*env)->DeleteGlobalRef(env, inst->context);
		}
	}

	free(inst);
}

xrt_result_t
xrt_instance_android_create(struct xrt_instance_info *ii, struct xrt_instance_android **out_xinst_android)
{
	struct _JavaVM *vm = ii->inst_info_android.vm;
	void *context = ii->inst_info_android.context;

	if (vm == NULL) {
		U_LOG_E("Invalid Java VM");
		return XRT_ERROR_ANDROID;
	}

	if (context == NULL) {
		U_LOG_E("Invalid context");
		return XRT_ERROR_ANDROID;
	}

	JNIEnv *env = NULL;
	if (vm->functions->AttachCurrentThread(&vm->functions, &env, NULL) != JNI_OK) {
		U_LOG_E("Failed to attach thread");
		return XRT_ERROR_ANDROID;
	}

	jobject global_context = (*env)->NewGlobalRef(env, context);
	if (global_context == NULL) {
		U_LOG_E("Failed to create global ref");
		return XRT_ERROR_ANDROID;
	}

	struct android_instance *inst = U_TYPED_CALLOC(struct android_instance);

	inst->vm = vm;
	inst->context = global_context;

	inst->base.get_vm = android_instance_get_vm;
	inst->base.get_context = android_instance_get_context;
	inst->base.register_activity_lifecycle_callback = android_instance_register_activity_lifecycle_callback;
	inst->base.remove_activity_lifecycle_callback = android_instance_remove_activity_lifecycle_callback;
	inst->base.destroy = android_instance_destroy;

	inst->lifecycle_callbacks = android_lifecycle_callbacks_create(&inst->base);

	*out_xinst_android = &inst->base;
	return XRT_SUCCESS;
}

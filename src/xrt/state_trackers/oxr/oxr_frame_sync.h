// Copyright 2024, Collabora, Ltd.
// Copyright 2024, QUALCOMM CORPORATION.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  The objects that handle session running status and blocking of xrWaitFrame.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Yulou Liu <quic_yuloliu@quicinc.com>
 * @ingroup oxr_main
 */

#pragma once

#include "xrt/xrt_openxr_includes.h"
#include "xrt/xrt_compiler.h"
#include "xrt/xrt_config_os.h"

#include "util/u_misc.h"
#include "util/u_logging.h"

#if defined(XRT_OS_LINUX) || defined(XRT_ENV_MINGW)
#include <pthread.h>
#include <semaphore.h>
#include <assert.h>
#define OS_THREAD_HAVE_SETNAME
#elif defined(XRT_OS_WINDOWS)
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <assert.h>
#define OS_THREAD_HAVE_SETNAME
#else
#error "OS not supported"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*! to be modified
 * All in one helper that handles locking, waiting for change
 */
struct os_synchronization_helper
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;

	bool canWaitFrameReturn;
	bool initialized;
	bool running;
};

/*!
 * Initialize the synchronization helper.
 *
 * @public @memberof os_synchronization_helper
 */
static inline int
os_synchronization_init(struct os_synchronization_helper *osh)
{
	U_ZERO(osh);

	int ret = pthread_mutex_init(&osh->mutex, NULL);
	if (ret != 0) {
		return ret;
	}

	ret = pthread_cond_init(&osh->cond, NULL);
	if (ret) {
		pthread_mutex_destroy(&osh->mutex);
		return ret;
	}
	osh->canWaitFrameReturn = 0;
	osh->initialized = true;
	osh->running = false;

	return 0;
}

/*!
 *
 *
 * @public @memberof os_synchronization_helper
 */
static inline XrResult
os_synchronization_wait(struct os_synchronization_helper *osh)
{
	pthread_mutex_lock(&osh->mutex);
	while (osh->running) {
		if (1 == osh->canWaitFrameReturn) {
			osh->canWaitFrameReturn = 0;
			break;
		} else if (0 == osh->canWaitFrameReturn) {
			pthread_cond_wait(&osh->cond, &osh->mutex);
			continue;
		} else {
			// we are not suppose to be here
			// print error message and return ??
			pthread_mutex_unlock(&osh->mutex);
			return XR_ERROR_SESSION_NOT_RUNNING;
		}
	}
	if (osh->running) {
		pthread_mutex_unlock(&osh->mutex);
		return XR_SUCCESS;
	} else {
		pthread_mutex_unlock(&osh->mutex);
		return XR_ERROR_SESSION_NOT_RUNNING;
	}
}

/*!
 *
 *
 * @public @memberof os_synchronization_helper
 */
static inline XrResult
os_synchronization_release(struct os_synchronization_helper *osh)
{
	pthread_mutex_lock(&osh->mutex);
	if (osh->running) {
		if (0 == osh->canWaitFrameReturn) {
			osh->canWaitFrameReturn = 1;
			pthread_cond_signal(&osh->cond);
			pthread_mutex_unlock(&osh->mutex);
			return XR_SUCCESS;
		}
	}
	pthread_mutex_unlock(&osh->mutex);
	return XR_ERROR_SESSION_NOT_RUNNING;
}

/*!
 *
 *
 * @public @memberof os_synchronization_helper
 */
static inline XrResult
os_synchronization_begin(struct os_synchronization_helper *osh)
{
	pthread_mutex_lock(&osh->mutex);
	if (!osh->running) {
		//any assumption on the value canWaitFrameReturn ?
		osh->canWaitFrameReturn = 1;
		osh->running = true;
		pthread_cond_signal(&osh->cond);
		pthread_mutex_unlock(&osh->mutex);
		return XR_SUCCESS;
	}
	// type of error to return ?
	pthread_mutex_unlock(&osh->mutex);
	return XR_ERROR_SESSION_NOT_RUNNING;
}

/*!
 *
 *
 * @public @memberof os_synchronization_helper
 */
static inline XrResult
os_synchronization_end(struct os_synchronization_helper *osh)
{
	pthread_mutex_lock(&osh->mutex);
	if (osh->running) {
		// whether to reset canWaitFrameReturn ?
		osh->running = false;
		pthread_cond_signal(&osh->cond);
		pthread_mutex_unlock(&osh->mutex);
		return XR_SUCCESS;
	}
	pthread_mutex_unlock(&osh->mutex);
	return XR_ERROR_SESSION_NOT_RUNNING;
}

/*!
 *
 *
 *
 *
 * @public @memberof os_synchronization_helper
 */
static inline XrResult
os_synchronization_destroy(struct os_synchronization_helper *osh)
{
	// The fields are protected.
	pthread_mutex_lock(&osh->mutex);
	assert(osh->initialized);

	if (osh->running) {
		// Stop the thread.
		osh->running = false;
		// Wake up the thread if it is waiting.
		pthread_cond_signal(&osh->cond);
	}

	// No longer need to protect fields.
	pthread_mutex_unlock(&osh->mutex);

	// Destroy resources.
	pthread_mutex_destroy(&osh->mutex);
	pthread_cond_destroy(&osh->cond);
	osh->canWaitFrameReturn = 0;
	osh->initialized = false;
	osh->running = false;
}

#ifdef __cplusplus
} // extern "C"
#endif
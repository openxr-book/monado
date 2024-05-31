// Copyright 2019-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Multi client wrapper compositor.
 * @author Pete Black <pblack@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup comp_multi
 */

#include "xrt/xrt_session.h"

#include "os/os_time.h"

#include "util/u_var.h"
#include "util/u_wait.h"
#include "util/u_misc.h"
#include "util/u_time.h"
#include "util/u_debug.h"
#include "util/u_handles.h"
#include "util/u_trace_marker.h"
#include "util/u_distortion_mesh.h"

#include "multi/comp_multi_private.h"

#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef XRT_GRAPHICS_SYNC_HANDLE_IS_FD
#include <unistd.h>
#endif

#ifdef XRT_OS_ANDROID
#include "android/android_custom_surface.h"
#include "android/android_globals.h"
#endif

/*
 *
 * Slot management functions.
 *
 */


/*!
 * Clear a slot, need to have the list_and_timing_lock held.
 */
static void
slot_clear_locked(struct multi_compositor *mc, struct multi_layer_slot *slot)
{
	if (slot->active) {
		uint64_t now_ns = os_monotonic_get_ns();
		u_pa_retired(mc->upa, slot->data.frame_id, now_ns);
	}

	for (size_t i = 0; i < slot->layer_count; i++) {
		for (size_t k = 0; k < ARRAY_SIZE(slot->layers[i].xscs); k++) {
			xrt_swapchain_reference(&slot->layers[i].xscs[k], NULL);
		}
	}

	U_ZERO(slot);
	slot->data.frame_id = -1;
}

/*!
 * Clear a slot, need to have the list_and_timing_lock held.
 */
static void
slot_move_into_cleared(struct multi_layer_slot *dst, struct multi_layer_slot *src)
{
	assert(!dst->active);
	assert(dst->data.frame_id == -1);

	// All references are kept.
	*dst = *src;

	U_ZERO(src);
	src->data.frame_id = -1;
}

/*!
 * Move a slot into a cleared slot, must be cleared before.
 */
static void
slot_move_and_clear_locked(struct multi_compositor *mc, struct multi_layer_slot *dst, struct multi_layer_slot *src)
{
	slot_clear_locked(mc, dst);
	slot_move_into_cleared(dst, src);
}


/*
 *
 * Event management functions.
 *
 */

xrt_result_t
multi_compositor_push_event(struct multi_compositor *mc, const union xrt_session_event *xse)
{
	// Dispatch to the current event sink.
	return xrt_session_event_sink_push(mc->xses, xse);
}


/*
 *
 * Wait helper thread.
 *
 */

static bool
is_pushed_or_waiting_locked(struct multi_compositor *mc)
{
	return mc->wait_thread.waiting ||     //
	       mc->wait_thread.xcf != NULL || //
	       mc->wait_thread.xcsem != NULL; //
}

static void
wait_fence(struct xrt_compositor_fence **xcf_ptr)
{
	COMP_TRACE_MARKER();
	xrt_result_t ret = XRT_SUCCESS;

	// 100ms
	uint64_t timeout_ns = 100 * U_TIME_1MS_IN_NS;

	do {
		ret = xrt_compositor_fence_wait(*xcf_ptr, timeout_ns);
		if (ret != XRT_TIMEOUT) {
			break;
		}

		U_LOG_W("Waiting on client fence timed out > 100ms!");
	} while (true);

	xrt_compositor_fence_destroy(xcf_ptr);

	if (ret != XRT_SUCCESS) {
		U_LOG_E("Fence waiting failed!");
	}
}

static void
wait_semaphore(struct xrt_compositor_semaphore **xcsem_ptr, uint64_t value)
{
	COMP_TRACE_MARKER();
	xrt_result_t ret = XRT_SUCCESS;

	// 100ms
	uint64_t timeout_ns = 100 * U_TIME_1MS_IN_NS;

	do {
		ret = xrt_compositor_semaphore_wait(*xcsem_ptr, value, timeout_ns);
		if (ret != XRT_TIMEOUT) {
			break;
		}

		U_LOG_W("Waiting on client semaphore value '%" PRIu64 "' timed out > 100ms!", value);
	} while (true);

	xrt_compositor_semaphore_reference(xcsem_ptr, NULL);

	if (ret != XRT_SUCCESS) {
		U_LOG_E("Semaphore waiting failed!");
	}
}

static void
wait_for_scheduled_free(struct multi_compositor *mc)
{
	COMP_TRACE_MARKER();

	os_mutex_lock(&mc->slot_lock);

	struct multi_compositor volatile *v_mc = mc;

	// Block here if the scheduled slot is not clear.
	while (v_mc->scheduled.active) {
		uint64_t now_ns = os_monotonic_get_ns();

		// This frame is for the next frame, drop the old one no matter what.
		if (time_is_within_half_ms(mc->progress.data.display_time_ns, mc->slot_next_frame_display)) {
			U_LOG_W("%.3fms: Dropping old missed frame in favour for completed new frame",
			        time_ns_to_ms_f(now_ns));
			break;
		}

		// Replace the scheduled frame if it's in the past.
		if (v_mc->scheduled.data.display_time_ns < now_ns) {
			U_LOG_T("%.3fms: Replacing frame for time in past in favour of completed new frame",
			        time_ns_to_ms_f(now_ns));
			break;
		}

		U_LOG_D(
		    "Two frames have completed GPU work and are waiting to be displayed."
		    "\n\tnext frame: %fms (%" PRIu64
		    ") (next time for compositor to pick up frame)"
		    "\n\tprogress: %fms (%" PRIu64
		    ")  (latest completed frame)"
		    "\n\tscheduled: %fms (%" PRIu64 ") (oldest waiting frame)",
		    time_ns_to_ms_f((int64_t)v_mc->slot_next_frame_display - now_ns),        //
		    v_mc->slot_next_frame_display,                                           //
		    time_ns_to_ms_f((int64_t)v_mc->progress.data.display_time_ns - now_ns),  //
		    v_mc->progress.data.display_time_ns,                                     //
		    time_ns_to_ms_f((int64_t)v_mc->scheduled.data.display_time_ns - now_ns), //
		    v_mc->scheduled.data.display_time_ns);                                   //

		os_mutex_unlock(&mc->slot_lock);

		os_precise_sleeper_nanosleep(&mc->scheduled_sleeper, U_TIME_1MS_IN_NS);

		os_mutex_lock(&mc->slot_lock);
	}

	os_mutex_unlock(&mc->slot_lock);

	/*
	 * Need to take list_and_timing_lock before slot_lock because slot_lock
	 * is taken in multi_compositor_deliver_any_frames with list_and_timing_lock
	 * held to stop clients from going away.
	 */
	os_mutex_lock(&mc->msc->list_and_timing_lock);
	os_mutex_lock(&mc->slot_lock);
	slot_move_and_clear_locked(mc, &mc->scheduled, &mc->progress);
	os_mutex_unlock(&mc->slot_lock);
	os_mutex_unlock(&mc->msc->list_and_timing_lock);
}

static void *
run_func(void *ptr)
{
	struct multi_compositor *mc = (struct multi_compositor *)ptr;

	U_TRACE_SET_THREAD_NAME("Multi Client Module: Waiter");
	os_thread_helper_name(&mc->wait_thread.oth, "Multi Client Module: Waiter");

	os_thread_helper_lock(&mc->wait_thread.oth);

	// Signal the start function that we are enterting the loop.
	mc->wait_thread.alive = true;
	os_thread_helper_signal_locked(&mc->wait_thread.oth);

	/*
	 * One can view the layer_commit function and the wait thread as a
	 * producer/consumer pair. This loop is the consumer side of that pair.
	 * We look for either a fence or a semaphore on each loop, if none are
	 * found we check if we are running then wait on the conditional
	 * variable once again waiting to be signalled by the producer.
	 */
	while (os_thread_helper_is_running_locked(&mc->wait_thread.oth)) {
		/*
		 * Here we wait for the either a semaphore or a fence, if
		 * neither has been set we wait/sleep here (again).
		 */
		if (mc->wait_thread.xcsem == NULL && mc->wait_thread.xcf == NULL) {
			// Spurious wakeups are handled below.
			os_thread_helper_wait_locked(&mc->wait_thread.oth);
			// Fall through here on stopping to clean up and outstanding waits.
		}

		int64_t frame_id = mc->wait_thread.frame_id;
		struct xrt_compositor_fence *xcf = mc->wait_thread.xcf;
		struct xrt_compositor_semaphore *xcsem = mc->wait_thread.xcsem; // No need to ref, a move.
		uint64_t value = mc->wait_thread.value;

		// Ok to clear these on spurious wakeup as they are empty then anyways.
		mc->wait_thread.frame_id = 0;
		mc->wait_thread.xcf = NULL;
		mc->wait_thread.xcsem = NULL;
		mc->wait_thread.value = 0;

		// We are being stopped, or a spurious wakeup, loop back and check running.
		if (xcf == NULL && xcsem == NULL) {
			continue;
		}

		// We now know that we should wait.
		mc->wait_thread.waiting = true;

		os_thread_helper_unlock(&mc->wait_thread.oth);

		if (xcsem != NULL) {
			wait_semaphore(&xcsem, value);
		}
		if (xcf != NULL) {
			wait_fence(&xcf);
		}

		// Sample time outside of lock.
		uint64_t now_ns = os_monotonic_get_ns();

		os_mutex_lock(&mc->msc->list_and_timing_lock);
		u_pa_mark_gpu_done(mc->upa, frame_id, now_ns);
		os_mutex_unlock(&mc->msc->list_and_timing_lock);

		// Wait for the delivery slot.
		wait_for_scheduled_free(mc);

		os_thread_helper_lock(&mc->wait_thread.oth);

		/*
		 * Finally no longer waiting, this must be done after
		 * wait_for_scheduled_free because it moves the slots/layers
		 * from progress to scheduled to be picked up by the compositor.
		 */
		mc->wait_thread.waiting = false;

		if (mc->wait_thread.blocked) {
			// Release one thread
			mc->wait_thread.blocked = false;
			os_thread_helper_signal_locked(&mc->wait_thread.oth);
		}
	}

	os_thread_helper_unlock(&mc->wait_thread.oth);

	return NULL;
}

static void
wait_for_wait_thread_locked(struct multi_compositor *mc)
{
	// Should we wait for the last frame.
	if (is_pushed_or_waiting_locked(mc)) {
		COMP_TRACE_IDENT(blocked);

		// There should only be one thread entering here.
		assert(mc->wait_thread.blocked == false);

		// OK, wait until the wait thread releases us by setting blocked to false
		mc->wait_thread.blocked = true;
		while (mc->wait_thread.blocked) {
			os_thread_helper_wait_locked(&mc->wait_thread.oth);
		}
	}
}

static void
wait_for_wait_thread(struct multi_compositor *mc)
{
	os_thread_helper_lock(&mc->wait_thread.oth);

	wait_for_wait_thread_locked(mc);

	os_thread_helper_unlock(&mc->wait_thread.oth);
}

static void
push_fence_to_wait_thread(struct multi_compositor *mc, int64_t frame_id, struct xrt_compositor_fence *xcf)
{
	os_thread_helper_lock(&mc->wait_thread.oth);

	// The function begin_layer should have waited, but just in case.
	assert(!mc->wait_thread.waiting);
	wait_for_wait_thread_locked(mc);

	assert(mc->wait_thread.xcf == NULL);

	mc->wait_thread.frame_id = frame_id;
	mc->wait_thread.xcf = xcf;

	os_thread_helper_signal_locked(&mc->wait_thread.oth);

	os_thread_helper_unlock(&mc->wait_thread.oth);
}

static void
push_semaphore_to_wait_thread(struct multi_compositor *mc,
                              int64_t frame_id,
                              struct xrt_compositor_semaphore *xcsem,
                              uint64_t value)
{
	os_thread_helper_lock(&mc->wait_thread.oth);

	// The function begin_layer should have waited, but just in case.
	assert(!mc->wait_thread.waiting);
	wait_for_wait_thread_locked(mc);

	assert(mc->wait_thread.xcsem == NULL);

	mc->wait_thread.frame_id = frame_id;
	xrt_compositor_semaphore_reference(&mc->wait_thread.xcsem, xcsem);
	mc->wait_thread.value = value;

	os_thread_helper_signal_locked(&mc->wait_thread.oth);

	os_thread_helper_unlock(&mc->wait_thread.oth);
}


/*
 *
 * Compositor functions.
 *
 */

static xrt_result_t
multi_compositor_get_swapchain_create_properties(struct xrt_compositor *xc,
                                                 const struct xrt_swapchain_create_info *info,
                                                 struct xrt_swapchain_create_properties *xsccp)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	return xrt_comp_get_swapchain_create_properties(&mc->msc->xcn->base, info, xsccp);
}

static xrt_result_t
multi_compositor_create_swapchain(struct xrt_compositor *xc,
                                  const struct xrt_swapchain_create_info *info,
                                  struct xrt_swapchain **out_xsc)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	return xrt_comp_create_swapchain(&mc->msc->xcn->base, info, out_xsc);
}

static xrt_result_t
multi_compositor_import_swapchain(struct xrt_compositor *xc,
                                  const struct xrt_swapchain_create_info *info,
                                  struct xrt_image_native *native_images,
                                  uint32_t image_count,
                                  struct xrt_swapchain **out_xsc)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	return xrt_comp_import_swapchain(&mc->msc->xcn->base, info, native_images, image_count, out_xsc);
}

static xrt_result_t
multi_compositor_import_fence(struct xrt_compositor *xc,
                              xrt_graphics_sync_handle_t handle,
                              struct xrt_compositor_fence **out_xcf)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	return xrt_comp_import_fence(&mc->msc->xcn->base, handle, out_xcf);
}

static xrt_result_t
multi_compositor_create_semaphore(struct xrt_compositor *xc,
                                  xrt_graphics_sync_handle_t *out_handle,
                                  struct xrt_compositor_semaphore **out_xcsem)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	// We don't wrap the semaphore and it's safe to pass it out directly.
	return xrt_comp_create_semaphore(&mc->msc->xcn->base, out_handle, out_xcsem);
}

static xrt_result_t
multi_compositor_begin_session(struct xrt_compositor *xc, const struct xrt_begin_session_info *info)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	assert(!mc->state.session_active);
	if (!mc->state.session_active) {
		multi_system_compositor_update_session_status(mc->msc, true);
		mc->state.session_active = true;
	}

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_end_session(struct xrt_compositor *xc)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	assert(mc->state.session_active);
	if (mc->state.session_active) {
		multi_system_compositor_update_session_status(mc->msc, false);
		mc->state.session_active = false;
	}

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_predict_frame(struct xrt_compositor *xc,
                               int64_t *out_frame_id,
                               uint64_t *out_wake_time_ns,
                               uint64_t *out_predicted_gpu_time_ns,
                               uint64_t *out_predicted_display_time_ns,
                               uint64_t *out_predicted_display_period_ns)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);
	uint64_t now_ns = os_monotonic_get_ns();
	os_mutex_lock(&mc->msc->list_and_timing_lock);

	u_pa_predict(                         //
	    mc->upa,                          //
	    now_ns,                           //
	    out_frame_id,                     //
	    out_wake_time_ns,                 //
	    out_predicted_display_time_ns,    //
	    out_predicted_display_period_ns); //

	os_mutex_unlock(&mc->msc->list_and_timing_lock);

	*out_predicted_gpu_time_ns = 0;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_mark_frame(struct xrt_compositor *xc,
                            int64_t frame_id,
                            enum xrt_compositor_frame_point point,
                            uint64_t when_ns)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	uint64_t now_ns = os_monotonic_get_ns();

	switch (point) {
	case XRT_COMPOSITOR_FRAME_POINT_WOKE:
		os_mutex_lock(&mc->msc->list_and_timing_lock);
		u_pa_mark_point(mc->upa, frame_id, U_TIMING_POINT_WAKE_UP, now_ns);
		os_mutex_unlock(&mc->msc->list_and_timing_lock);
		break;
	default: assert(false);
	}

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_wait_frame(struct xrt_compositor *xc,
                            int64_t *out_frame_id,
                            uint64_t *out_predicted_display_time_ns,
                            uint64_t *out_predicted_display_period_ns)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	int64_t frame_id = -1;
	uint64_t wake_up_time_ns = 0;
	uint64_t predicted_gpu_time_ns = 0;

	xrt_comp_predict_frame(               //
	    xc,                               //
	    &frame_id,                        //
	    &wake_up_time_ns,                 //
	    &predicted_gpu_time_ns,           //
	    out_predicted_display_time_ns,    //
	    out_predicted_display_period_ns); //

	// Wait until the given wake up time.
	u_wait_until(&mc->frame_sleeper, wake_up_time_ns);

	uint64_t now_ns = os_monotonic_get_ns();

	// Signal that we woke up.
	xrt_comp_mark_frame(xc, frame_id, XRT_COMPOSITOR_FRAME_POINT_WOKE, now_ns);

	*out_frame_id = frame_id;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_begin_frame(struct xrt_compositor *xc, int64_t frame_id)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	os_mutex_lock(&mc->msc->list_and_timing_lock);
	uint64_t now_ns = os_monotonic_get_ns();
	u_pa_mark_point(mc->upa, frame_id, U_TIMING_POINT_BEGIN, now_ns);
	os_mutex_unlock(&mc->msc->list_and_timing_lock);

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_discard_frame(struct xrt_compositor *xc, int64_t frame_id)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);
	uint64_t now_ns = os_monotonic_get_ns();

	os_mutex_lock(&mc->msc->list_and_timing_lock);
	u_pa_mark_discarded(mc->upa, frame_id, now_ns);
	os_mutex_unlock(&mc->msc->list_and_timing_lock);

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_begin(struct xrt_compositor *xc, const struct xrt_layer_frame_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	// As early as possible.
	uint64_t now_ns = os_monotonic_get_ns();
	os_mutex_lock(&mc->msc->list_and_timing_lock);
	u_pa_mark_delivered(mc->upa, data->frame_id, now_ns, data->display_time_ns);
	os_mutex_unlock(&mc->msc->list_and_timing_lock);

	/*
	 * We have to block here for the waiting thread to push the last
	 * submitted frame from the progress slot to the scheduled slot,
	 * it only does after the sync object has signaled completion.
	 *
	 * If the previous frame's GPU work has not completed that means we
	 * will block here, but that is okay as the app has already submitted
	 * the GPU for this frame. This should have very little impact on GPU
	 * utilisation, if any.
	 */
	wait_for_wait_thread(mc);

	assert(mc->progress.layer_count == 0);
	U_ZERO(&mc->progress);

	mc->progress.active = true;
	mc->progress.data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_projection(struct xrt_compositor *xc,
                                  struct xrt_device *xdev,
                                  struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
                                  const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);
	(void)mc;

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;
	for (uint32_t i = 0; i < data->view_count; ++i) {
		xrt_swapchain_reference(&mc->progress.layers[index].xscs[i], xsc[i]);
	}
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_projection_depth(struct xrt_compositor *xc,
                                        struct xrt_device *xdev,
                                        struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
                                        struct xrt_swapchain *d_xsc[XRT_MAX_VIEWS],
                                        const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;

	for (uint32_t i = 0; i < data->view_count; ++i) {
		xrt_swapchain_reference(&mc->progress.layers[index].xscs[i], xsc[i]);
		xrt_swapchain_reference(&mc->progress.layers[index].xscs[i + data->view_count], d_xsc[i]);
	}
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_quad(struct xrt_compositor *xc,
                            struct xrt_device *xdev,
                            struct xrt_swapchain *xsc,
                            const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;
	xrt_swapchain_reference(&mc->progress.layers[index].xscs[0], xsc);
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_cube(struct xrt_compositor *xc,
                            struct xrt_device *xdev,
                            struct xrt_swapchain *xsc,
                            const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;
	xrt_swapchain_reference(&mc->progress.layers[index].xscs[0], xsc);
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_cylinder(struct xrt_compositor *xc,
                                struct xrt_device *xdev,
                                struct xrt_swapchain *xsc,
                                const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;
	xrt_swapchain_reference(&mc->progress.layers[index].xscs[0], xsc);
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_equirect1(struct xrt_compositor *xc,
                                 struct xrt_device *xdev,
                                 struct xrt_swapchain *xsc,
                                 const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;
	xrt_swapchain_reference(&mc->progress.layers[index].xscs[0], xsc);
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_equirect2(struct xrt_compositor *xc,
                                 struct xrt_device *xdev,
                                 struct xrt_swapchain *xsc,
                                 const struct xrt_layer_data *data)
{
	struct multi_compositor *mc = multi_compositor(xc);

	size_t index = mc->progress.layer_count++;
	mc->progress.layers[index].xdev = xdev;
	xrt_swapchain_reference(&mc->progress.layers[index].xscs[0], xsc);
	mc->progress.layers[index].data = *data;

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_commit(struct xrt_compositor *xc, xrt_graphics_sync_handle_t sync_handle)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);
	struct xrt_compositor_fence *xcf = NULL;
	int64_t frame_id = mc->progress.data.frame_id;

	do {
		if (!xrt_graphics_sync_handle_is_valid(sync_handle)) {
			break;
		}

		xrt_result_t xret = xrt_comp_import_fence( //
		    &mc->msc->xcn->base,                   //
		    sync_handle,                           //
		    &xcf);                                 //
		/*!
		 * If import_fence succeeded, we have transferred ownership to
		 * the compositor no need to do anything more. If the call
		 * failed we need to close the handle.
		 */
		if (xret == XRT_SUCCESS) {
			break;
		}

		u_graphics_sync_unref(&sync_handle);
	} while (false); // Goto without the labels.

	if (xcf != NULL) {
		push_fence_to_wait_thread(mc, frame_id, xcf);
	} else {
		// Assume that the app side compositor waited.
		uint64_t now_ns = os_monotonic_get_ns();

		os_mutex_lock(&mc->msc->list_and_timing_lock);
		u_pa_mark_gpu_done(mc->upa, frame_id, now_ns);
		os_mutex_unlock(&mc->msc->list_and_timing_lock);

		wait_for_scheduled_free(mc);
	}

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_layer_commit_with_semaphore(struct xrt_compositor *xc,
                                             struct xrt_compositor_semaphore *xcsem,
                                             uint64_t value)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);
	int64_t frame_id = mc->progress.data.frame_id;

	push_semaphore_to_wait_thread(mc, frame_id, xcsem, value);

	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_set_thread_hint(struct xrt_compositor *xc, enum xrt_thread_hint hint, uint32_t thread_id)
{
	// No-op
	return XRT_SUCCESS;
}

static xrt_result_t
multi_compositor_get_display_refresh_rate(struct xrt_compositor *xc, float *out_display_refresh_rate_hz)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	return xrt_comp_get_display_refresh_rate(&mc->msc->xcn->base, out_display_refresh_rate_hz);
}

static xrt_result_t
multi_compositor_request_display_refresh_rate(struct xrt_compositor *xc, float display_refresh_rate_hz)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	xrt_comp_request_display_refresh_rate(&mc->msc->xcn->base, display_refresh_rate_hz);

#ifdef XRT_OS_ANDROID
	// TODO: notify the display refresh changed event by android display callback function.
	float current_refresh_rate_hz =
	    android_custom_surface_get_display_refresh_rate(android_globals_get_vm(), android_globals_get_context());

	if (current_refresh_rate_hz != 0 && current_refresh_rate_hz != mc->current_refresh_rate_hz) {
		xrt_syscomp_notify_display_refresh_changed(&mc->msc->base, xc, mc->current_refresh_rate_hz,
		                                           current_refresh_rate_hz);
		mc->current_refresh_rate_hz = current_refresh_rate_hz;
	}
#endif

	return XRT_SUCCESS;
}

static void
multi_compositor_destroy(struct xrt_compositor *xc)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = multi_compositor(xc);

	if (mc->state.session_active) {
		multi_system_compositor_update_session_status(mc->msc, false);
		mc->state.session_active = false;
	}

	os_mutex_lock(&mc->msc->list_and_timing_lock);

	// Remove it from the list of clients.
	for (size_t i = 0; i < MULTI_MAX_CLIENTS; i++) {
		if (mc->msc->clients[i] == mc) {
			mc->msc->clients[i] = NULL;
		}
	}

	os_mutex_unlock(&mc->msc->list_and_timing_lock);

	// Destroy the wait thread, destroy also stops the thread.
	os_thread_helper_destroy(&mc->wait_thread.oth);

	// We are now off the rendering list, clear slots for any swapchains.
	os_mutex_lock(&mc->msc->list_and_timing_lock);
	slot_clear_locked(mc, &mc->progress);
	slot_clear_locked(mc, &mc->scheduled);
	slot_clear_locked(mc, &mc->delivered);
	os_mutex_unlock(&mc->msc->list_and_timing_lock);

	// Does null checking.
	u_pa_destroy(&mc->upa);

	os_precise_sleeper_deinit(&mc->frame_sleeper);
	os_precise_sleeper_deinit(&mc->scheduled_sleeper);

	os_mutex_destroy(&mc->slot_lock);

	free(mc);
}

static void
log_frame_time_diff(uint64_t frame_time_ns, uint64_t display_time_ns)
{
	int64_t diff_ns = (int64_t)frame_time_ns - (int64_t)display_time_ns;
	bool late = false;
	if (diff_ns < 0) {
		diff_ns = -diff_ns;
		late = true;
	}

	U_LOG_W("Frame %s by %.2fms!", late ? "late" : "early", time_ns_to_ms_f(diff_ns));
}

void
multi_compositor_deliver_any_frames(struct multi_compositor *mc, uint64_t display_time_ns)
{
	os_mutex_lock(&mc->slot_lock);

	if (!mc->scheduled.active) {
		os_mutex_unlock(&mc->slot_lock);
		return;
	}

	if (time_is_greater_then_or_within_half_ms(display_time_ns, mc->scheduled.data.display_time_ns)) {
		slot_move_and_clear_locked(mc, &mc->delivered, &mc->scheduled);

		uint64_t frame_time_ns = mc->delivered.data.display_time_ns;
		if (!time_is_within_half_ms(frame_time_ns, display_time_ns)) {
			log_frame_time_diff(frame_time_ns, display_time_ns);
		}
	}

	os_mutex_unlock(&mc->slot_lock);
}

void
multi_compositor_latch_frame_locked(struct multi_compositor *mc, uint64_t when_ns, int64_t system_frame_id)
{
	u_pa_latched(mc->upa, mc->delivered.data.frame_id, when_ns, system_frame_id);
}

void
multi_compositor_retire_delivered_locked(struct multi_compositor *mc, uint64_t when_ns)
{
	slot_clear_locked(mc, &mc->delivered);
}

xrt_result_t
multi_compositor_create(struct multi_system_compositor *msc,
                        const struct xrt_session_info *xsi,
                        struct xrt_session_event_sink *xses,
                        struct xrt_compositor_native **out_xcn)
{
	COMP_TRACE_MARKER();

	struct multi_compositor *mc = U_TYPED_CALLOC(struct multi_compositor);

	mc->base.base.get_swapchain_create_properties = multi_compositor_get_swapchain_create_properties;
	mc->base.base.create_swapchain = multi_compositor_create_swapchain;
	mc->base.base.import_swapchain = multi_compositor_import_swapchain;
	mc->base.base.import_fence = multi_compositor_import_fence;
	mc->base.base.create_semaphore = multi_compositor_create_semaphore;
	mc->base.base.begin_session = multi_compositor_begin_session;
	mc->base.base.end_session = multi_compositor_end_session;
	mc->base.base.predict_frame = multi_compositor_predict_frame;
	mc->base.base.mark_frame = multi_compositor_mark_frame;
	mc->base.base.wait_frame = multi_compositor_wait_frame;
	mc->base.base.begin_frame = multi_compositor_begin_frame;
	mc->base.base.discard_frame = multi_compositor_discard_frame;
	mc->base.base.layer_begin = multi_compositor_layer_begin;
	mc->base.base.layer_projection = multi_compositor_layer_projection;
	mc->base.base.layer_projection_depth = multi_compositor_layer_projection_depth;
	mc->base.base.layer_quad = multi_compositor_layer_quad;
	mc->base.base.layer_cube = multi_compositor_layer_cube;
	mc->base.base.layer_cylinder = multi_compositor_layer_cylinder;
	mc->base.base.layer_equirect1 = multi_compositor_layer_equirect1;
	mc->base.base.layer_equirect2 = multi_compositor_layer_equirect2;
	mc->base.base.layer_commit = multi_compositor_layer_commit;
	mc->base.base.layer_commit_with_semaphore = multi_compositor_layer_commit_with_semaphore;
	mc->base.base.destroy = multi_compositor_destroy;
	mc->base.base.set_thread_hint = multi_compositor_set_thread_hint;
	mc->base.base.get_display_refresh_rate = multi_compositor_get_display_refresh_rate;
	mc->base.base.request_display_refresh_rate = multi_compositor_request_display_refresh_rate;
	mc->msc = msc;
	mc->xses = xses;
	mc->xsi = *xsi;

	os_mutex_init(&mc->slot_lock);
	os_thread_helper_init(&mc->wait_thread.oth);

	// Passthrough our formats from the native compositor to the client.
	mc->base.base.info = msc->xcn->base.info;

	// Used in wait frame.
	os_precise_sleeper_init(&mc->frame_sleeper);

	// Used in scheduled waiting function.
	os_precise_sleeper_init(&mc->scheduled_sleeper);

	// This is safe to do without a lock since we are not on the list yet.
	u_paf_create(msc->upaf, &mc->upa);

	os_mutex_lock(&msc->list_and_timing_lock);

	// If we have too many clients, just ignore it.
	for (size_t i = 0; i < MULTI_MAX_CLIENTS; i++) {
		if (mc->msc->clients[i] != NULL) {
			continue;
		}
		mc->msc->clients[i] = mc;
		break;
	}

	u_pa_info(                                         //
	    mc->upa,                                       //
	    msc->last_timings.predicted_display_time_ns,   //
	    msc->last_timings.predicted_display_period_ns, //
	    msc->last_timings.diff_ns);                    //

	os_mutex_unlock(&msc->list_and_timing_lock);

	// Last start the wait thread.
	os_thread_helper_start(&mc->wait_thread.oth, run_func, mc);

	os_thread_helper_lock(&mc->wait_thread.oth);

	// Wait for the wait thread to fully start.
	while (!mc->wait_thread.alive) {
		os_thread_helper_wait_locked(&mc->wait_thread.oth);
	}

	os_thread_helper_unlock(&mc->wait_thread.oth);

#ifdef XRT_OS_ANDROID
	mc->current_refresh_rate_hz =
	    android_custom_surface_get_display_refresh_rate(android_globals_get_vm(), android_globals_get_context());
#endif

	*out_xcn = &mc->base;

	return XRT_SUCCESS;
}

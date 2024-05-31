// Copyright 2024, Gavin John
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Driver for Xreal Light glasses.
 * @author Gavin John <gavinnjohn@gmail.com>
 * @ingroup drv_xreal_light
 */

#include "xreal_light_interface.h"

#include "xrt/xrt_defines.h"
#include "xrt/xrt_compiler.h"

#include "os/os_hid.h"
#include "os/os_time.h"
#include "os/os_threading.h"

#include "math/m_api.h"
#include "math/m_imu_3dof.h"
#include "math/m_mathinclude.h"
#include "math/m_relation_history.h"

#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_distortion_mesh.h"
#include "util/u_trace_marker.h"
#include "util/u_var.h"

#ifdef XRT_OS_LINUX
#include "util/u_linux.h"
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XREAL_LIGHT_DEBUG(hmd, ...) U_LOG_XDEV_IFL_D(&hmd->base, hmd->log_level, __VA_ARGS__)
#define XREAL_LIGHT_TRACE(hmd, ...) U_LOG_XDEV_IFL_T(&hmd->base, hmd->log_level, __VA_ARGS__)
#define XREAL_LIGHT_WARN(hmd, ...) U_LOG_XDEV_IFL_W(&hmd->base, hmd->log_level, __VA_ARGS__)
#define XREAL_LIGHT_ERROR(hmd, ...) U_LOG_XDEV_IFL_E(&hmd->base, hmd->log_level, __VA_ARGS__)

DEBUG_GET_ONCE_LOG_OPTION(xreal_light_log, "XREAL_LIGHT_LOG", U_LOGGING_DEBUG);

/*!
 * Private struct for the xreal_light device.
 *
 * @ingroup drv_xreal_light
 * @implements xrt_device
 */
struct xreal_light_hmd
{
	struct xrt_device base;

	//! The log level for this device.
	enum u_logging_level log_level;

	//! Thread for continually reading from the device and sending keep alive packets.
	struct os_thread_helper oth;

	//! Mutex for MCU and OV580 access.
	struct os_mutex device_mutex;

	//! Protected by the device_mutex.
	struct os_hid_device *mcu_hid_handle;
	struct os_hid_device *ov580_hid_handle;

	//! Monado tracker helpers.
	struct m_imu_3dof fusion;
	struct m_relation_history *relation_hist;

	//! Keep alive packet sending.
	timepoint_ns last_heartbeat_sent_time;
	timepoint_ns last_heartbeat_ack_time;
};

static inline struct xreal_light_hmd *
xreal_light_hmd(struct xrt_device *dev)
{
	return (struct xreal_light_hmd *)dev;
}

static void
xreal_light_hmd_get_tracked_pose(struct xrt_device *xdev,
                                 enum xrt_input_name name,
                                 uint64_t at_timestamp_ns,
                                 struct xrt_space_relation *out_relation)
{
	struct xreal_light_hmd *hmd = xreal_light_hmd(xdev);

	struct xrt_space_relation relation;
	U_ZERO(&relation); // Clear out the relation.

	switch (name) {
		case XRT_INPUT_GENERIC_HEAD_POSE: {
			const enum xrt_space_relation_flags flags = (enum xrt_space_relation_flags)(
				XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);

			relation.relation_flags = flags;

			m_relation_history_get(hmd->relation_hist, at_timestamp_ns, &relation);
			relation.relation_flags = flags; // Needed after history_get

			*out_relation = relation;

			math_quat_normalize(&out_relation->pose.orientation);
			return;
		};
		default:
			return;
	}
}

static bool
xreal_light_hmd_compute_distortion(
    struct xrt_device *xdev, uint32_t view, float u, float v, struct xrt_uv_triplet *result)
{
	return u_compute_distortion_none(u, v, result);
}

static void
teardown(struct xreal_light_hmd *hmd)
{
	// Stop the variable tracking.
	u_var_remove_root(hmd);

	// Shutdown the sensor thread early.
	os_thread_helper_stop_and_wait(&hmd->oth);

	if (hmd->mcu_hid_handle != NULL) {
		os_hid_destroy(hmd->mcu_hid_handle);
		hmd->mcu_hid_handle = NULL;
	}

	if (hmd->ov580_hid_handle != NULL) {
		os_hid_destroy(hmd->ov580_hid_handle);
		hmd->ov580_hid_handle = NULL;
	}

	m_relation_history_destroy(&hmd->relation_hist);

	// Destroy the fusion.
	m_imu_3dof_close(&hmd->fusion);

	os_thread_helper_destroy(&hmd->oth);
	os_mutex_destroy(&hmd->device_mutex);
}

static void
xreal_light_hmd_destroy(struct xrt_device *xdev)
{
	struct xreal_light_hmd *hmd = xreal_light_hmd(xdev);
	teardown(hmd);

	u_device_free(&hmd->base);
}

static bool
send_heartbeat(struct xreal_light_hmd *hmd)
{
	uint8_t buf[2] = {0x40, 0x4B};

	os_mutex_lock(&hmd->device_mutex);

	bool ret = os_hid_write(hmd->mcu_hid_handle, buf, sizeof(buf));

	if (!ret) {
		XREAL_LIGHT_ERROR(hmd, "Failed to send keep alive packet");
	}

	os_mutex_unlock(&hmd->device_mutex);

	return ret;
}

static bool
send_display_mode(struct xreal_light_hmd *hmd, uint8_t mode)
{
	uint8_t buf[3] = {0x31, 0x33, mode};

	os_mutex_lock(&hmd->device_mutex);

	bool ret = os_hid_write(hmd->mcu_hid_handle, buf, sizeof(buf));

	if (!ret) {
		XREAL_LIGHT_ERROR(hmd, "Failed to set display mode");
	}

	os_mutex_unlock(&hmd->device_mutex);

	return ret;
}

static bool
send_imu_streaming(struct xreal_light_hmd *hmd, bool enable)
{
	uint8_t buf[2] = {0x19, enable ? 0x01 : 0x00};

	os_mutex_lock(&hmd->device_mutex);

	bool ret = os_hid_write(hmd->mcu_hid_handle, buf, sizeof(buf));

	if (!ret) {
		XREAL_LIGHT_ERROR(hmd, "Failed to set IMU streaming");
	}

	os_mutex_unlock(&hmd->device_mutex);

	return ret;
}

static void
handle_mcu_msg(struct xreal_light_hmd *hmd, uint8_t *buffer, int size)
{
	char str[1024];
	for (int i = 0; i < size; i++) {
		snprintf(str + i * 3, 4, "%02X ", buffer[i]);
	}
	XREAL_LIGHT_DEBUG(hmd, "Received MCU message: %s", str);
}

static void
handle_ov580_msg(struct xreal_light_hmd *hmd, uint8_t *buffer, int size)
{
	char str[1024];
	for (int i = 0; i < size; i++) {
		snprintf(str + i * 3, 4, "%02X ", buffer[i]);
	}
	XREAL_LIGHT_DEBUG(hmd, "Received OV580 message: %s", str);
}

static int
read_one_mcu_packet(struct xreal_light_hmd *hmd)
{
	uint8_t buffer[XREAL_LIGHT_MCU_DATA_BUFFER_SIZE];

	int size = os_hid_read(hmd->mcu_hid_handle, buffer, XREAL_LIGHT_MCU_DATA_BUFFER_SIZE, 0);
	if (size <= 0) {
		return size;
	}

	handle_mcu_msg(hmd, buffer, size);
	return 0;
}

static int
read_one_ov580_packet(struct xreal_light_hmd *hmd)
{
	uint8_t buffer[XREAL_LIGHT_OV580_DATA_BUFFER_SIZE];

	int size = os_hid_read(hmd->ov580_hid_handle, buffer, XREAL_LIGHT_OV580_DATA_BUFFER_SIZE, 0);
	if (size <= 0) {
		return size;
	}

	handle_ov580_msg(hmd, buffer, size);
	return 0;
}

static void *
read_thread(void *ptr)
{
	U_TRACE_SET_THREAD_NAME("Xreal Light HMD Read Thread");

#ifdef XRT_OS_LINUX
	// Try to raise priority of this thread, so we don't miss packets under load
	u_linux_try_to_set_realtime_priority_on_thread(U_LOGGING_INFO, "Xreal Light HMD Read Thread");
#endif

	struct xreal_light_hmd *hmd = (struct xreal_light_hmd *)ptr;

	os_thread_helper_lock(&hmd->oth);

	timepoint_ns now = os_monotonic_get_ns();
	
	if (now - hmd->last_heartbeat_sent_time > time_s_to_ns(XREAL_LIGHT_HEARTBEAT_INTERVAL_MS)) {
		send_heartbeat(hmd);

		hmd->last_heartbeat_sent_time = now;
	}

	int ret = 0;

	while (os_thread_helper_is_running_locked(&hmd->oth) && ret >= 0) {
		os_thread_helper_unlock(&hmd->oth);

		ret = read_one_mcu_packet(hmd);

		os_thread_helper_lock(&hmd->oth);
	}

	ret = 0;

	while (os_thread_helper_is_running_locked(&hmd->oth) && ret >= 0) {
		os_thread_helper_unlock(&hmd->oth);

		ret = read_one_ov580_packet(hmd);

		os_thread_helper_lock(&hmd->oth);
	}

	os_thread_helper_unlock(&hmd->oth);

	return NULL;
}

struct xrt_device *
xreal_light_hmd_create_device(struct os_hid_device *mcu_hid_handle,
                              struct os_hid_device *ov580_hid_handle)
{
	// Initialize HMD device
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct xreal_light_hmd *hmd =
	    U_DEVICE_ALLOCATE(struct xreal_light_hmd, flags, 1, 0); // TODO: Figure out what the outputs here mean
	
	// Check for errors
	if (mcu_hid_handle == NULL || ov580_hid_handle == NULL) {
		XREAL_LIGHT_ERROR(hmd, "Failed to open HID devices");
		return NULL;
	}

	// Set log level
	hmd->log_level = debug_get_log_option_xreal_light_log();

	// Assign HID handles
	hmd->mcu_hid_handle = mcu_hid_handle;
	hmd->ov580_hid_handle = ov580_hid_handle;

	// Create thread and mutex immediately
	os_mutex_init(&hmd->device_mutex);
	os_thread_helper_init(&hmd->oth);

	// Set static device properties.
	snprintf(hmd->base.str, XRT_DEVICE_NAME_LEN, "Xreal Light Glasses");
	snprintf(hmd->base.serial, XRT_DEVICE_NAME_LEN, "Xreal Light Glasses");
	hmd->base.hmd->screens[0].nominal_frame_interval_ns = time_s_to_ns(1.0f / 60.0f); // 60 Hz display
	u_distortion_mesh_set_none(&hmd->base); // No distortion correction

	// Describe device capabilities.
	hmd->base.name = XRT_DEVICE_GENERIC_HMD;
	hmd->base.device_type = XRT_DEVICE_TYPE_HMD;
	hmd->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;
	hmd->base.orientation_tracking_supported = true;
	hmd->base.position_tracking_supported = false; // TODO: Support 6DoF tracking

	// Device functions
	hmd->base.update_inputs = u_device_noop_update_inputs;
	hmd->base.get_tracked_pose = xreal_light_hmd_get_tracked_pose;
	hmd->base.get_view_poses = u_device_get_view_poses;
	hmd->base.compute_distortion = xreal_light_hmd_compute_distortion;
	hmd->base.destroy = xreal_light_hmd_destroy;

	// Set up Monado tracker helpers.
	m_imu_3dof_init(&hmd->fusion, M_IMU_3DOF_USE_GRAVITY_DUR_20MS);
	m_relation_history_create(&hmd->relation_hist);

	// Start the sensor threads.
	if (os_thread_helper_start(&hmd->oth, read_thread, (void *)hmd) < 0) {
		XREAL_LIGHT_ERROR(hmd, "Failed to start sensor thread");
		goto cleanup;
	}

	// Finally, do the startup sequence.
	if (!send_imu_streaming(hmd, false)) {
		XREAL_LIGHT_ERROR(hmd, "Failed to disable IMU streaming (to read configuration data)");
		goto cleanup;
	}

	if (!send_display_mode(hmd, XREAL_LIGHT_DISPLAY_MODE_HIGH_REFRESH_RATE_SBS)) {
		XREAL_LIGHT_ERROR(hmd, "Failed to set high refresh rate SBS mode");
		goto cleanup;
	}

	if (!send_imu_streaming(hmd, true)) {
		XREAL_LIGHT_ERROR(hmd, "Failed to enable IMU streaming");
		goto cleanup;
	}

	// Return
	return &hmd->base;
cleanup:
	teardown(hmd);
	free(hmd);

	return NULL;
}

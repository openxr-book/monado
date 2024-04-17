// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface to nolo driver.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup drv_nolo
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include "util/u_logging.h"
#include "xrt/xrt_prober.h"
#include "xrt/xrt_defines.h"
#include "math/m_imu_3dof.h"
#include "os/os_hid.h"
#include "nolo_fusion.h"

struct nolo_imu_range_modes_report
{
	uint8_t id;
	uint8_t gyro_range;
	uint8_t accel_range;
	uint8_t unknown[61];
} __attribute__((packed));

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @defgroup drv_nolo Nolo driver
 * @ingroup drv
 *
 * @brief A nolo driver, that detects by USB VID/PID plus product name.
 * and thus exposes an "auto-prober" to explicitly discover the device.
 *
 * This device has an implementation of @ref xrt_auto_prober to perform hardware
 * detection, as well as an implementation of @ref xrt_device for the actual device.
 *
 * If your device is or has USB HID that **can** be detected based on USB VID/PID,
 * you can skip the @ref xrt_auto_prober implementation, and instead implement a
 * "found" function that matches the signature expected by xrt_prober_entry::found.
 * See for example @ref hdk_found.
 */

//iManufacturer           1 LYRobotix
#define NOLO_VID 0x0483
//iProduct                2 NOLO HMD
#define NOLO_PID 0x5750

enum nolo_input_index
{
	NOLO_TRACKPAD_CLICK,
	NOLO_TRIGGER_CLICK,
	NOLO_MENU_CLICK,
	NOLO_SYSTEM_CLICK,
	NOLO_SQUEEZE_CLICK,
	NOLO_TRACKPAD_TOUCH,
	NOLO_TRACKPAD,
	NOLO_GRIP_POSE,
	NOLO_AIM_POSE,

	NOLO_MAX_INDEX
};


enum nolo_device_type
{
	NOLO_CONTROLLER,
	NOLO_TRACKER,
};

typedef struct
{
	int16_t accel[3];
	int16_t gyro[3];
	int16_t rot[10];
	int16_t w;
	uint64_t tick;
} nolo_sample;


typedef enum
{
	//LEGACY firmware < 2.0
	NOLO_LEGACY_CONTROLLER_TRACKER = 165,
	NOLO_LEGACY_HMD_TRACKER = 166,
	//firmware > 2.0
	NOLO_CONTROLLER_0_HMD_SMP1 = 16,
	NOLO_CONTROLLER_1_HMD_SMP2 = 17,
} nolo_irq_cmd;

/*!
 * A nolo device.
 *
 * @implements xrt_device
 */
struct nolo_device
{
	struct xrt_device base;

	struct xrt_pose pose;

	enum nolo_device_type nolo_type;

	enum u_logging_level log_level;

	nolo_sample sample;

	struct xrt_vec3 raw_accel, raw_gyro, home_position, last_home_position;
	vec3f raw_accel_fusion, raw_gyro_fusion;

	struct m_imu_3dof fusion;
	fusion sensor_fusion;

	float two_point_drift_angle;

	struct
	{
		double acc_range;
		double gyro_range;
		struct xrt_vec3 acc_bias;
		struct xrt_vec3 acc_scale;
		struct xrt_vec3 gyro_bias;
		struct xrt_vec3 gyro_scale;

		//! IMU position in tracking space.
		struct xrt_pose trackref;
	} imu;

	/*
	 * The firmware version
	 */
	int revision;

	/*
	 * For parsing the controller inputs
	 */
	float controller_values[8];

	/*
	 * Data from USB
	 */
	struct os_hid_device *data_hid;

	/*
	 * Manages all nolo devices.
	 */
	struct nolo_system *sys;

	//timepoint_ns last_imu_device_time_ns;
	//timepoint_ns last_imu_local_time_ns;
	/*
	 * The battery charge level 
	 * -1 means not connected
	 * 106 means charging I think ... have to do more testing
	 */
	int8_t battery;

	/*
	 * F7 (247) means connnected
	 */
	uint8_t connected;

	/*
	 * Time codes provided in usb packet
	 */
	uint8_t tick;
	uint64_t monotonic_ticks_per_sec;

	uint64_t tick64;

	int8_t version_id;

	/*
	 * The last time in nano seconds the system button was pressed.
	 */
	uint64_t system_click_last_time;

	uint64_t system_click_time_between_clicks;

};



/*
 * The inputs for the controllers are routed through the hmd tracker.
 */
struct nolo_system
{
	struct nolo_device * hmd_tracker;
	struct nolo_device * left_controller;
	struct nolo_device * right_controller;

	/*
	 * The number of devices connected.
	 */
	int num_devices;
};



/*!
 * Probing function for Nolo devices.
 *
 * @ingroup drv_nolo
 * @see xrt_prober_found_func_t
 */
int
nolo_found(struct xrt_prober *xp,
          struct xrt_prober_device **devices,
          size_t device_count,
          size_t index,
          cJSON *attached_data,
          struct xrt_device **out_xdev);

/*!
 * Create a nolo device.
 *
 * @ingroup drv_nolo
 */
struct nolo_device *
nolo_device_create(struct os_hid_device *hid, enum nolo_device_type nolo_type);

/*!
 * @dir drivers/nolo
 *
 * @brief @ref drv_nolo files.
 */

void btea_decrypt(uint32_t *v, int n, int base_rounds, uint32_t const key[4]);
void nolo_decrypt_data(unsigned char* buf);

void nolo_decode_base_station(struct nolo_device* priv, const unsigned char* data);
void nolo_decode_hmd_marker(struct nolo_device* priv, const unsigned char* data);
void nolo_decode_controller(struct nolo_device* priv, const unsigned char* data);
void nolo_decode_quat_orientation(const unsigned char* data, struct xrt_quat* quat);
void nolo_decode_hmd_orientation(const unsigned char* data, nolo_sample* smp);
void nolo_decode_position(const unsigned char* data, struct xrt_vec3* pos);


#ifdef __cplusplus
}
#endif

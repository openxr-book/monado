// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Provides driver support for the Nolo CV1 head tracker and controllers.
 *
 *
 * Losely based on the Hydra driver because all data is routed through the
 * nolo head tracker via USB.  The controllers are wireless and shouldn't be 
 * connected via USB.  
 * 
 * If the Controllers are connected via USB, this driver will ignore the 
 * directly connected device.  However, the controllers will still work
 * (in the case you need to charge).
 * 
 * The base station is also ignored by this driver.  So the base station
 * can be connected for charging while in use.
 *
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Yann Vernier
 * @author Michael Speth <mspeth@monky-games.com>
 * @ingroup drv_nolo
 */

#include "xrt/xrt_device.h"

#include "os/os_hid.h"
#include "os/os_time.h"

#include "math/m_api.h"
#include "math/m_mathinclude.h"

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_time.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_logging.h"
#include "util/u_distortion_mesh.h"
#include "util/u_json.h"
#include "nolo_interface.h"
#include "nolo_bindings.h"
#include "nolo_debug.h"
#include "nolo_fusion.h"
#include "util/u_prober.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

DEBUG_GET_ONCE_LOG_OPTION(nolo_log, "NOLO_LOG", U_LOGGING_WARN)

/*
 *
 * Structs and defines.
 *
 */

#define NOLO_TICKS_PER_SECOND (1000000000)

/*
 * 
 * The first controller detected, will be left.
 * 
 */
bool has_detected_left_controller = false;

#define NOLO_CONTROLLER_INITIAL_POS(is_left) (struct xrt_vec3){(is_left) ? -0.2f : 0.2f, -0.3f, -0.5f}

/*
 *
 */
#define SET_INPUT(NAME)                                                                                            \
	device->base.inputs[NOLO_##NAME].name = XRT_INPUT_NOLO_##NAME;                                               \

// Casting helper function
static inline struct nolo_device *
nolo_device(struct xrt_device *xdev)
{
	return (struct nolo_device *)xdev;
}


#define DEG_TO_RAD(D) ((D)*M_PI / 180.)

/*
 * Returns true if this device is the left controller and false otherwise.
 */
static bool is_left_controller(struct nolo_device *device){
	if(device->sys->left_controller == device){
		return true;
	}
	return false;
}

/*
 * Returns true if this device is the right controller and false otherwise.
 */
static bool is_right_controller(struct nolo_device *device){
	if(device->sys->right_controller == device){
		return true;
	}
	return false;
}

static void
nolo_device_set_digital(struct nolo_device *virtual_device, struct nolo_device *physical_device, timepoint_ns now, int index)
{
	virtual_device->base.inputs[index].timestamp = now;
	virtual_device->base.inputs[index].value.boolean = physical_device->controller_values[index];
}

static void
nolo_device_set_trackpad_X(struct nolo_device *virtual_device, struct nolo_device *physical_device, timepoint_ns now, int index)
{
	virtual_device->base.inputs[NOLO_TRACKPAD].timestamp = now;
	virtual_device->base.inputs[NOLO_TRACKPAD].value.vec2.x = (127.5f - physical_device->controller_values[index])/127.5f;
}

static void
nolo_device_set_trackpad_Y(struct nolo_device *virtual_device, struct nolo_device *physical_device, timepoint_ns now, int index)
{
	virtual_device->base.inputs[NOLO_TRACKPAD].timestamp = now;
	virtual_device->base.inputs[NOLO_TRACKPAD].value.vec2.y = (127.5f - physical_device->controller_values[index])/127.5f;
}

static void
nolo_device_destroy(struct xrt_device *xdev)
{
	struct nolo_device *device = nolo_device(xdev);
	struct nolo_system *sys = device->sys;

	NOLO_DEBUG(device,"Destroy %s %s %d",device->base.str,device->base.serial,device->base.device_type);

	if(strcmp(device->base.str,"Nolo Tracker") == 0){
		sys->hmd_tracker = NULL;
		// only 1 hid
		os_hid_destroy(device->data_hid);

	}else if(strcmp(device->base.str,"Nolo Left Controller") == 0){
		sys->left_controller = NULL;

	}else if(strcmp(device->base.str,"Nolo Right Controller") == 0){
		sys->right_controller = NULL;
	}

	m_imu_3dof_close(&device->fusion);

	device->sys = NULL;

	// Remove the variable tracking.
	u_var_remove_root(device);

	u_device_free(&device->base);

	// REMOVE System
	sys->num_devices--;
	if(sys->num_devices == 0){
		free(sys);
	}
}


/**
 * Recenter's the tracker and controllers.
 */
static void recenter(struct nolo_device * device){
	struct nolo_system *ns = device->sys;
	ofusion_init(&ns->hmd_tracker->sensor_fusion);
	ofusion_init(&ns->left_controller->sensor_fusion);
	ofusion_init(&ns->right_controller->sensor_fusion);
}

uint64_t ohmd_monotonic_conv(uint64_t ticks, uint64_t srcTicksPerSecond, uint64_t dstTicksPerSecond)
{
	// This would be more straightforward with floating point arithmetic,
	// but we avoid it here in order to avoid the rounding errors that that
	// introduces. Also, by splitting out the units in this way, we're able
	// to deal with much larger values before running into problems with
	// integer overflow.
	return ticks / srcTicksPerSecond * dstTicksPerSecond +
		ticks % srcTicksPerSecond * dstTicksPerSecond / srcTicksPerSecond;
}

static const uint64_t NUM_1_000_000_000 = 1000000000;
void ohmd_monotonic_init(struct nolo_device* device)
{
		struct timespec ts;
		if (clock_getres(CLOCK_MONOTONIC, &ts) !=  0) {
			device->monotonic_ticks_per_sec = NUM_1_000_000_000;
			return;
		}

		device->monotonic_ticks_per_sec =
			ts.tv_nsec >= 1000 ?
			NUM_1_000_000_000 :
			NUM_1_000_000_000 / ts.tv_nsec;
}

uint64_t ohmd_monotonic_get(struct nolo_device* device)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	return ohmd_monotonic_conv(
		now.tv_sec * NUM_1_000_000_000 + now.tv_nsec,
		NUM_1_000_000_000,
		device->monotonic_ticks_per_sec);
}

double ohmd_get_tick()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (double)now.tv_sec * 1.0 + (double)now.tv_usec / 1000000.0;
}

// How to address DRIFT
// haagch — 01/28/2023 8:32 PM
// presumably the rotation will drift over time as is
// the way to solve it is to write code that filters the imu and the pose together such that the movement directions 
// detected by the accelerometer are lined up with the movement direction of the positional tracking

static void handle_tracker_sensor_msg(struct nolo_device* device, unsigned char* buffer, int size, int type, timepoint_ns now)
{
	uint64_t last_tick = device->sample.tick;

	switch(type) {
		case 0: nolo_decode_hmd_marker(device, buffer); break;
		case 1: nolo_decode_controller(device, buffer); break;
	}

	device->sample.tick = os_monotonic_get_ns();

	// Startup correction, ignore last_sample_tick if zero.
	uint64_t tick_delta = 0;
	if(last_tick > 0) //startup correction
		tick_delta = device->sample.tick - last_tick;

	float dt = (tick_delta/(float)device->monotonic_ticks_per_sec)/1000.0f;

	vec3f mag = {{0.0f, 0.0f, 0.0f}};

	device->raw_gyro_fusion.x = device->raw_gyro.x;
	device->raw_gyro_fusion.y = device->raw_gyro.y;
	device->raw_gyro_fusion.z = device->raw_gyro.z;

	device->raw_accel_fusion.x = device->raw_accel.x;
	device->raw_accel_fusion.y = device->raw_accel.y;
	device->raw_accel_fusion.z = device->raw_accel.z;

	ofusion_update(&device->sensor_fusion, dt, &device->raw_gyro_fusion, &device->raw_accel_fusion, &mag);

	device->pose.orientation.x = device->sensor_fusion.orient.x;
	device->pose.orientation.y = device->sensor_fusion.orient.y;
	device->pose.orientation.z = device->sensor_fusion.orient.z;
	device->pose.orientation.w = device->sensor_fusion.orient.w;

	// print out accell and gyro
	if(PLOT_HMD && device->nolo_type == NOLO_TRACKER){
		print_plot_data(device);
	}else if(PLOT_CONTROLLER1 && device->nolo_type == NOLO_CONTROLLER && is_left_controller(device)){
		print_plot_data(device);
	}else if(PLOT_CONTROLLER2 && device->nolo_type == NOLO_CONTROLLER && is_right_controller(device)){
		print_plot_data(device);
	}

	//print_nolo_tracker_rotation_full(device);
	print_nolo_tracker_rotation(device);
	print_nolo_tracker_position(device);

	// check if need to correct for drift
	// doesn't really work
	/*
	if(device->nolo_type == NOLO_TRACKER){
		if(device->home_position.x != device->last_home_position.x){
			recenter(device);
			device->last_home_position.x = device->home_position.x;
			device->last_home_position.y = device->home_position.y;
			device->last_home_position.z = device->home_position.z;
		}
	}
	*/
}

/*
 * @param read_type if 0 only read controller button presses and 1 is for position/orientation reads.
 */
static int
nolo_read_data_hid(struct nolo_device *device, timepoint_ns now)
{
	assert(device);
	uint8_t buffer[256];
	//unsigned char buffer[256];
	bool got_message = false;
	//NOLO_DEBUG(device,"data hid = %p",(void *)&device->data_hid);
	do {
		int ret = os_hid_read(device->data_hid, buffer, sizeof(buffer), 0);
		if (ret < 0) {
			return ret;
		}
		if (ret == 0) {
			return got_message ? 1 : 0;
		}

		nolo_decrypt_data(buffer);

		// currently the only message type the hardware supports
		switch (buffer[0]) {
			case NOLO_CONTROLLER_0_HMD_SMP1:
			{
				handle_tracker_sensor_msg(device->sys->left_controller, buffer, ret, 1, now);
				handle_tracker_sensor_msg(device->sys->hmd_tracker, buffer, ret, 0, now);
				break;
			}
			case NOLO_CONTROLLER_1_HMD_SMP2:
			{
				handle_tracker_sensor_msg(device->sys->right_controller, buffer, ret, 1, now);
				// no reason to decode tracker data as it is a duplicate of controller 1's packet.
				// test if it is a duplicate of data
				// print out hmd tracker data
				//handle_tracker_sensor_msg(device->sys->hmd_tracker, buffer, ret, 0, now);
				break;
			}
			default:
				NOLO_ERROR(device,"unknown message type: %u", buffer[0]);
		}
	} while (true);

	return 0;
}

/*
 * Update the internal state of the nolo driver.
 *
 * Reads devices, checks the state machine and timeouts, etc.
 *
 */
static int
nolo_system_update(struct nolo_device *device)
{
	assert(device);
	timepoint_ns now = os_monotonic_get_ns();

	// In all states of the state machine:
	// Try reading a report: will only return >0 if we get a full motion
	// report.
    nolo_read_data_hid(device, now);

	return 0;
}

static void
nolo_device_update_inputs(struct xrt_device *xdev)
{
	struct nolo_device *device = nolo_device(xdev);
	// Empty, you should put code to update the attached input fields (if any)

	timepoint_ns now = os_monotonic_get_ns();


	if (device->nolo_type != NOLO_TRACKER){
		// only run the update on the real device.
		nolo_system_update(device);
	}

	// READ USB data and decode the device
	// only do this for the hmd because 
	// will get the data for the controllers too
	//TODO Consider only doing this 1 time
	//nolo_read_data_hid(device->sys->hmd_tracker, now);
	//nolo_read_data_hid(device);
	//nolo_system_update(device);

	NOLO_DEBUG_INPUT(device->sys->left_controller,"Controller - %ld",now);
	nolo_device_set_digital(device->sys->left_controller,device,now,NOLO_TRACKPAD_CLICK);
	nolo_device_set_digital(device->sys->left_controller,device,now,NOLO_TRIGGER_CLICK);
	nolo_device_set_digital(device->sys->left_controller,device,now,NOLO_MENU_CLICK);
	nolo_device_set_digital(device->sys->left_controller,device,now,NOLO_SYSTEM_CLICK);
	nolo_device_set_digital(device->sys->left_controller,device,now,NOLO_SQUEEZE_CLICK);
	nolo_device_set_digital(device->sys->left_controller,device,now,NOLO_TRACKPAD_TOUCH);
	nolo_device_set_trackpad_X(device->sys->left_controller, device,now, 6);
	nolo_device_set_trackpad_Y(device->sys->left_controller, device,now, 7);
	print_controller_inputs(device->sys->left_controller);

	NOLO_DEBUG_INPUT(device->sys->right_controller,"Controller - %ld",now);
	nolo_device_set_digital(device->sys->right_controller,device,now,NOLO_TRACKPAD_CLICK);
	nolo_device_set_digital(device->sys->right_controller,device,now,NOLO_TRIGGER_CLICK);
	nolo_device_set_digital(device->sys->right_controller,device,now,NOLO_MENU_CLICK);
	nolo_device_set_digital(device->sys->right_controller,device,now,NOLO_SYSTEM_CLICK);
	nolo_device_set_digital(device->sys->right_controller,device,now,NOLO_SQUEEZE_CLICK);
	nolo_device_set_digital(device->sys->right_controller,device,now,NOLO_TRACKPAD_TOUCH);
	nolo_device_set_trackpad_X(device->sys->right_controller, device,now, 6);
	nolo_device_set_trackpad_Y(device->sys->right_controller, device,now, 7);
	print_controller_inputs(device->sys->right_controller);

    print_nolo_controller_trigger_pulled(device);

	// check if home button pressed twice fast 
	if(device->nolo_type == NOLO_CONTROLLER && device->base.device_type == XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER){
		if(device->sys->left_controller->base.inputs[NOLO_SYSTEM_CLICK].value.boolean){
			uint64_t last_system_click_time = device->sys->left_controller->system_click_last_time;
			// check if this is held down or first time clicked
			if(last_system_click_time == 0){
				device->sys->left_controller->system_click_last_time = now;

				// check if there was a button press before
				if(device->system_click_time_between_clicks != 0){
					uint64_t time_diff = now - device->system_click_time_between_clicks;
					float time_diff_ms = time_diff/1000000;
					//NOLO_DEBUG(device,"time between clicks = %f",time_diff_ms);
					if(time_diff_ms < 150){
						NOLO_DEBUG(device,"double click - system button");
						recenter(device);
					}
				}
			}
		}else{
			uint64_t last_system_click_time = device->system_click_last_time;
			if(last_system_click_time != 0){
				device->system_click_last_time = 0;
				device->system_click_time_between_clicks = now;
			}
		}
	}
}

static void
nolo_device_get_tracked_pose(struct xrt_device *xdev,
                            enum xrt_input_name name,
                            uint64_t at_timestamp_ns,
                            struct xrt_space_relation *out_relation)
{
	struct nolo_device *device = nolo_device(xdev);

	// maybe only do this for input updates??
	nolo_system_update(device);


	// TODO - Set offset by either grip or aim pose
	if (name == NOLO_AIM_POSE) {
		out_relation->pose = device->pose;
		//TODO need to translate the rotation and position to the ball
		// See psmv_push_pose_offset in xrt/drivers/psmv/psmv_driver.c
		//y += PSMV_BALL_FROM_IMU_Y_M;
		//y += PSMV_BALL_DIAMETER_M / 2.0;

	}else if (name == NOLO_GRIP_POSE) {
		out_relation->pose = device->pose;
	}else {
		out_relation->pose = device->pose;
	}

	//TODO Consider only doing this 1 time
	//nolo_read_data_hid(device->sys->hmd_tracker, at_timestamp_ns);
	//nolo_read_data_hid(device, at_timestamp_ns,1);

	// Estimate pose at timestamp at_timestamp_ns!
	//math_quat_normalize(&device->pose.orientation);
	//out_relation->pose = device->pose;
	out_relation->relation_flags = (enum xrt_space_relation_flags)( XRT_SPACE_RELATION_POSITION_VALID_BIT |
																	XRT_SPACE_RELATION_POSITION_TRACKED_BIT |
	    															XRT_SPACE_RELATION_ORIENTATION_VALID_BIT | 
																	XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);
}

static void
nolo_device_get_view_poses(struct xrt_device *xdev,
                          const struct xrt_vec3 *default_eye_relation,
                          uint64_t at_timestamp_ns,
                          uint32_t view_count,
                          struct xrt_space_relation *out_head_relation,
                          struct xrt_fov *out_fovs,
                          struct xrt_pose *out_poses)
{
	u_device_get_view_poses(xdev, default_eye_relation, at_timestamp_ns, view_count, out_head_relation, out_fovs, out_poses);
}

struct nolo_device *
nolo_device_create(struct os_hid_device *hid, enum nolo_device_type nolo_type)
{
	// This indicates you won't be using Monado's built-in tracking algorithms.
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);

	struct nolo_device * device;

	switch(nolo_type){
		case NOLO_TRACKER:
			device = U_DEVICE_ALLOCATE(struct nolo_device, flags, 1, 0);
			break;
		case NOLO_CONTROLLER:
			device = U_DEVICE_ALLOCATE(struct nolo_device, flags, 8, 0);
			break;
		default:
			return NULL;
	}

	// This list should be ordered, most preferred first.
	device->base.update_inputs    = nolo_device_update_inputs;
	device->base.get_tracked_pose = nolo_device_get_tracked_pose;
	device->base.get_view_poses   = nolo_device_get_view_poses;
	device->base.destroy          = nolo_device_destroy;
	device->data_hid              = hid;
	device->nolo_type             = nolo_type;
	device->pose                  = (struct xrt_pose)XRT_POSE_IDENTITY;
	device->log_level             = debug_get_log_option_nolo_log();

	//TODO find the firmware version
	device->revision = 2;

	// setting for recenter.
	device->system_click_last_time = 0;
	device->system_click_time_between_clicks = 0;

	// Initialize fusion
	m_imu_3dof_init(&device->fusion, M_IMU_3DOF_USE_GRAVITY_DUR_20MS);
	ohmd_monotonic_init(device);

	device->imu.gyro_range = 8.726646f;
	device->imu.acc_range = 39.226600f;

	device->imu.acc_scale.x = 1.0f;
	device->imu.acc_scale.y = 1.0f;
	device->imu.acc_scale.z = 1.0f;
	device->imu.gyro_scale.x = 1.0f;
	device->imu.gyro_scale.y = 1.0f;
	device->imu.gyro_scale.z = 1.0f;

	device->imu.acc_bias.x = 0.0f;
	device->imu.acc_bias.y = 0.0f;
	device->imu.acc_bias.z = 0.0f;
	device->imu.gyro_bias.x = 0.0f;
	device->imu.gyro_bias.y = 0.0f;
	device->imu.gyro_bias.z = 0.0f;

	switch(nolo_type){
		case NOLO_TRACKER:
			NOLO_DEBUG(device,"creating a tracker");
			device->base.name = XRT_DEVICE_NOLO_TRACKER;
			device->base.device_type = XRT_DEVICE_TYPE_GENERIC_TRACKER;
			SET_INPUT(GRIP_POSE);
			SET_INPUT(AIM_POSE);

			snprintf(device->base.str, XRT_DEVICE_NAME_LEN, "Nolo Tracker");
			//TODO get the real serial number
			snprintf(device->base.serial, XRT_DEVICE_NAME_LEN, "Nolo Tracker");

			break;
		case NOLO_CONTROLLER:
			NOLO_DEBUG(device,"creating a controller");
			device->base.name = XRT_DEVICE_NOLO_CONTROLLER;
			//TODO get the real serial number
			if(!has_detected_left_controller){
				NOLO_DEBUG(device,"left controller");
				device->base.device_type = XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER;
				has_detected_left_controller = true;
				snprintf(device->base.str, XRT_DEVICE_NAME_LEN, "Nolo Left Controller");
				snprintf(device->base.serial, XRT_DEVICE_NAME_LEN, "Nolo Left Controller");
			}else{
				NOLO_DEBUG(device,"right controller");
				device->base.device_type = XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER;
				snprintf(device->base.str, XRT_DEVICE_NAME_LEN, "Nolo Right Controller");
				snprintf(device->base.serial, XRT_DEVICE_NAME_LEN, "Nolo Right Controller");
			}
			SET_INPUT(TRACKPAD_CLICK);
			print_nolo_controller_trigger_pulled(device);
			SET_INPUT(TRIGGER_CLICK);
			SET_INPUT(MENU_CLICK);
			SET_INPUT(SYSTEM_CLICK);
			SET_INPUT(SQUEEZE_CLICK);
			SET_INPUT(TRACKPAD_TOUCH);
			SET_INPUT(TRACKPAD);
			SET_INPUT(GRIP_POSE);
			SET_INPUT(AIM_POSE);
			break;
		default:
			NOLO_ERROR(device,"Invalid Nolo Device");
	}
	device->base.orientation_tracking_supported = true;
	device->base.position_tracking_supported = false;

	device->base.binding_profiles = binding_profiles_nolo;
	device->base.binding_profile_count = binding_profiles_nolo_count;

	// Setup variable tracker: Optional but useful for debugging
	u_var_add_root(device, "Nolo", true);
	u_var_add_pose(device, &device->pose, "pose");
	u_var_add_log_level(device, &device->log_level, "log_level");

	return device;
}

int
nolo_found(struct xrt_prober *xp,
          struct xrt_prober_device **devices,
          size_t device_count,
          size_t index,
          cJSON *attached_data,
          struct xrt_device **out_xdev)
{
	struct xrt_prober_device *dev = devices[index];

	// get the product name in order to identify components
	unsigned char product_name[128];
	int ret = xrt_prober_get_string_descriptor(
	    xp,
		dev,           
	    XRT_PROBER_STRING_PRODUCT,
	    product_name,
	    sizeof(product_name));

    print_device_info(dev,product_name);

	unsigned char buf[256] = {0};
	int result = xrt_prober_get_string_descriptor(xp, dev, XRT_PROBER_STRING_PRODUCT, buf, sizeof(buf));

    // make sure it is a nolo device
    if(dev->vendor_id == NOLO_VID && dev->product_id == NOLO_PID){

		U_LOG_D("Vendor_ID(%d) & Product_ID(%d) & Product_Name(%s)",dev->vendor_id,dev->product_id,product_name);

        //TODO check the firmware version

        // check if the device is an hmd
        if(strcmp(product_name,"NOLO HMD") == 0){

            U_LOG_D("Found The HMD Tracker");

			// this is main nolo device that all inputs route through
			struct nolo_system *ns = U_TYPED_CALLOC(struct nolo_system);
			ns->num_devices = 0;

            struct os_hid_device *hmd_hid = NULL;
            //struct os_hid_device *controller_hid = NULL;

            // Interface 0 is the HID interface.
            result = xrt_prober_open_hid_interface(xp, dev, 0, &hmd_hid);
            if (result != 0) {
                U_LOG_D("Failed to open hmd_hid interface");
                return -1;
            }

            ns->hmd_tracker = nolo_device_create(hmd_hid, NOLO_TRACKER);
            if (ns->hmd_tracker == NULL) {
                U_LOG_D("Failed to create nolo hmd tracker");
                return -1;
            }
            ns->hmd_tracker->data_hid = hmd_hid;
			ns->hmd_tracker->sys = ns;
			ns->num_devices++;
			out_xdev[0] = &(ns->hmd_tracker->base);


			// create left and right controllers
            ns->left_controller = nolo_device_create(hmd_hid, NOLO_CONTROLLER);
            if (ns->left_controller == NULL) {
                U_LOG_D("Failed to create nolo left controller");
                return -1;
            }
			ns->left_controller->sys = ns;
			ns->num_devices++;
			out_xdev[1] = &(ns->left_controller->base);

            ns->right_controller = nolo_device_create(hmd_hid, NOLO_CONTROLLER);
            if (ns->right_controller == NULL) {
                U_LOG_D("Failed to create nolo right controller");
                return -1;
            }
			ns->right_controller->sys = ns;
			ns->num_devices++;
			out_xdev[2] = &(ns->right_controller->base);

			// init fusion
			ofusion_init(&ns->hmd_tracker->sensor_fusion);
			ofusion_init(&ns->left_controller->sensor_fusion);
			ofusion_init(&ns->right_controller->sensor_fusion);

            return 3;

        } else if(strcmp(product_name,"NOLO CONTROLLER") == 0){
            U_LOG_D("Controller is directly plugged in via usb, ignore %s",product_name);
        } else {
            U_LOG_D("Failed to add %s",product_name);
        }
    }else{
        U_LOG_D("Not a nolo device %s",product_name);
    }

    return -1;
}
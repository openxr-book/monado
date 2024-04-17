
#include "nolo_interface.h"
#include "nolo_debug.h"
#include "util/u_prober.h"

void print_device_info(struct xrt_prober_device *dev, unsigned char product_name[128])
{

	//1: 0x0483:0x5750
	//usb.product:      NOLO HMD
	//usb.manufacturer: LYRobotix
	U_LOG_I("====== Nolo device ======");
	U_LOG_I("Vendor:   %04x", dev->vendor_id);
	U_LOG_I("Product:  %04x", dev->product_id);
	U_LOG_I("Product:  %s", product_name);
	U_LOG_I("Class:    %d", dev->usb_dev_class);
	U_LOG_I("Bus type: %s", u_prober_bus_type_to_string(dev->bus));
}

void print_digital_input(struct nolo_device * device, int index)
{

	char name[100];	
	switch(device->base.inputs[index].name)
	{
		case XRT_INPUT_NOLO_SYSTEM_CLICK:   strcpy(name , "XRT_INPUT_NOLO_SYSTEM_CLICK"); break;
		case XRT_INPUT_NOLO_SQUEEZE_CLICK:  strcpy(name , "XRT_INPUT_NOLO_SQUEEZE_CLICK"); break;
		case XRT_INPUT_NOLO_MENU_CLICK:     strcpy(name , "XRT_INPUT_NOLO_MENU_CLICK"); break;
		case XRT_INPUT_NOLO_TRIGGER_CLICK:  strcpy(name , "XRT_INPUT_NOLO_TRIGGER_CLICK"); break;
		case XRT_INPUT_NOLO_TRACKPAD:       strcpy(name , "XRT_INPUT_NOLO_TRACKPAD"); break;
		case XRT_INPUT_NOLO_TRACKPAD_CLICK: strcpy(name , "XRT_INPUT_NOLO_CLICK"); break;
		case XRT_INPUT_NOLO_TRACKPAD_TOUCH: strcpy(name , "XRT_INPUT_NOLO_TOUCH"); break;
		case XRT_INPUT_NOLO_GRIP_POSE:      strcpy(name , "XRT_INPUT_NOLO_GRIP_POSE"); break;
		case XRT_INPUT_NOLO_AIM_POSE:       strcpy(name , "XRT_INPUT_NOLO_AIM_POSE"); break;
		default: strcpy(name , "unknown");
	}


	// check if trigger was pulled
	NOLO_DEBUG_INPUT(device,"%30s with index = %2d   mapping value = %10lf   boolean value = %1d",
		name,
		index,
		device->controller_values[index],
		device->base.inputs[index].value.boolean);
}

void print_analog_input(struct nolo_device * device, int index_x, int index_y)
{

	char name[100];	
	if(device->base.inputs[index_x].name == XRT_INPUT_NOLO_TRACKPAD)
	{
		strcpy(name , "XRT_INPUT_NOLO_TRACKPAD"); 
	}else{
		strcpy(name , "unknown");
	}


	// check if trigger was pulled
	NOLO_DEBUG_INPUT(device,"%30s  mapping value_x = %10lf   value = %1lf",
		name,
		device->controller_values[index_x],
		device->base.inputs[index_x].value.vec2.x);
	NOLO_DEBUG_INPUT(device,"%30s  mapping value_y = %10lf   value = %1lf",
		name,
		device->controller_values[index_y],
		device->base.inputs[index_x].value.vec2.y);
}

void print_controller_inputs(struct nolo_device *device)
{
	/*
	switch(device->base.device_type)
	{
		case XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER:
			NOLO_DEBUG_INPUT(device,"Nolo Left Controller Inputs");
			break;
		case XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER:
			NOLO_DEBUG_INPUT(device,"Nolo Right Controller Inputs");
			break;
		defaut:
			NOLO_DEBUG_INPUT(device,"HMD?");
	}
	*/
	print_digital_input(device,NOLO_TRACKPAD_CLICK);
	print_digital_input(device,NOLO_TRIGGER_CLICK);
	print_digital_input(device,NOLO_MENU_CLICK);
	print_digital_input(device,NOLO_SYSTEM_CLICK);
	print_digital_input(device,NOLO_SQUEEZE_CLICK);
	print_digital_input(device,NOLO_TRACKPAD_TOUCH);
 	print_analog_input(device, NOLO_TRACKPAD, 7);
    //nolo_device_set_trackpad_X(device, now, 6);
    //nolo_device_set_trackpad_Y(device, now, 7);
}

/**
 * Prints the tracker data position
*/
void print_nolo_tracker_position(struct nolo_device *device){
	if(device->nolo_type == NOLO_TRACKER){
        NOLO_DEBUG_POSITION(device,"Tracker[ pos(%4g,%4g,%4g)]",device->pose.position.x,device->pose.position.y,device->pose.position.z);
	}
}

/**
 * Prints the tracker data rotation
*/
void print_nolo_tracker_rotation(struct nolo_device *device){
	if(device->nolo_type == NOLO_TRACKER){
        //NOLO_DEBUG_ROTATION(device,"Tracker[ rot(%4g,%4g,%4g,%4g)]",device->fusion.rot.x,device->fusion.rot.y,device->fusion.rot.z,device->fusion.rot.w);
        NOLO_DEBUG_ROTATION(device,"Tracker[ rot(%4g,%4g,%4g,%4g)]",device->sensor_fusion.orient.x,device->sensor_fusion.orient.y,device->sensor_fusion.orient.z,device->sensor_fusion.orient.w);
	}
}

/**
 * Prints the tracker data full rotation information.
*/
void print_nolo_tracker_rotation_full(struct nolo_device *device){
	if(device->nolo_type == NOLO_TRACKER){
        NOLO_DEBUG_ROTATION(device,"Tracker[rawA(%8g,%8g,%8g) rawG(%8g,%8g,%8g) rot(%4g,%4g,%4g,%4g)]",
        device->raw_accel.x,device->raw_accel.y,device->raw_accel.z,
        device->raw_gyro.x,device->raw_gyro.y,device->raw_gyro.z,
        device->fusion.rot.x,device->fusion.rot.y,device->fusion.rot.z,device->fusion.rot.w);
	}
}

void print_nolo_controller_trigger_pulled(struct nolo_device *device){
    // check if trigger was pulled
	if(device->nolo_type == NOLO_CONTROLLER){
        NOLO_DEBUG_INPUT(device,"Trigger Click mapping value = %lf boolean value = %d",
            device->controller_values[NOLO_TRIGGER_CLICK],
            device->base.inputs[NOLO_TRIGGER_CLICK].value.boolean);
    }
}

/*
 * Prints the raw acceleration and gyroscope values to the console.
 */
void print_plot_data(struct nolo_device *device){
//	/*
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		device->raw_accel.x,device->raw_accel.y,device->raw_accel.z,
		device->raw_gyro.x,device->raw_gyro.y,device->raw_gyro.z
	);
//	*/
	/*
	// plot the position values
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->pose.position.x,
		(float)device->pose.position.y,
		(float)device->pose.position.z
	);
	*/
	/*
	// plot the rotation of HMD
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.w,
		(float)device->sample.accel[0],
		(float)device->sample.accel[1],
		(float)device->sample.accel[2],
		(float)device->sample.gyro[0],
		(float)device->sample.gyro[1],
		(float)device->sample.gyro[2]
	);
	*/

	/*
	// plot the rotation of HMD
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f,%05f,%05f,%05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[0],
		(float)device->sample.rot[1],
		(float)device->sample.rot[2],
		(float)device->sample.rot[3],
		(float)device->sample.rot[4],
		(float)device->sample.rot[5],
		(float)device->sample.rot[6],
		(float)device->sample.rot[7],
		(float)device->sample.rot[8],
		(float)device->sample.rot[9]
	);
	*/
	/*
	// plot the rotation of HMD
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[7],
		(float)device->sample.rot[8],
		(float)device->sample.rot[9]
	);
	*/
	/*
	// plot the rotation of HMD
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[6],
		(float)device->sample.rot[7],
		(float)device->sample.rot[8],
		(float)device->sample.rot[9]
	);
	*/
	/*
	// plot the rotation of HMD
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[3],
		(float)device->sample.rot[4],
		(float)device->sample.rot[5]
	);
	*/
	/*
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[0],
		(float)device->sample.rot[1],
		(float)device->sample.rot[2]
	);
	*/
	
	/*
	// plot the position values
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->connected,
		(float)device->battery,
		(float)device->time0,
		(float)device->time1
	);
	*/
	/*
	// plot the rotation of HMD
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->pose.orientation.x,
		(float)device->pose.orientation.y,
		(float)device->pose.orientation.z,
		(float)device->pose.orientation.w
	);
	*/
	/*
	// plot the rotation of Controller
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f,%05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[0],
		(float)device->sample.rot[1],
		(float)device->sample.rot[2],
		(float)device->sample.rot[3],
		(float)device->sample.rot[4],
		(float)device->sample.rot[5]
	);
	*/
	/*
	// plot the rotation of Controller
	NOLO_DEBUG_PLOT("%ld, %05f,%05f,%05f",
		os_realtime_get_ns(),
		(float)device->sample.rot[0],
		(float)device->sample.rot[1],
		(float)device->sample.rot[2]
	);
	*/
}
// Copyright 2017, Joey Ferwerda.
// SPDX-License-Identifier: BSL-1.0
/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 * Original implementation by: Yann Vernier.
 */

/* NOLO VR - Packet Decoding and Utilities */

#include <stdio.h>
#include "nolo_interface.h"
#include "nolo_debug.h"
#include "math/m_mathinclude.h"
#include "math/m_api.h"
#include "math/m_permutation.h"
#include "math/m_imu_3dof.h"

#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

#define POSITION_SCALE (0.0001)

#define CRYPT_WORDS (64-4)/4
#define CRYPT_OFFSET 1

inline static uint8_t read8(const unsigned char** buffer)
{
	uint8_t ret = **buffer;
	*buffer += 1;
	return ret;
}

inline static int16_t read16(const unsigned char** buffer)
{
	int16_t ret = **buffer | (*(*buffer + 1) << 8);
	*buffer += 2;
	return ret;
}

inline static uint32_t read32(const unsigned char** buffer)
{
	uint32_t ret = **buffer | (*(*buffer + 1) << 8) | (*(*buffer + 2) << 16) | (*(*buffer + 3) << 24);
	*buffer += 4;
	return ret;
}

void btea_decrypt(uint32_t *v, int n, int base_rounds, uint32_t const key[4])
{
	uint32_t y, z, sum;
	unsigned p, rounds, e;

	/* Decoding Part */
	rounds = base_rounds + 52/n;
	sum = rounds*DELTA;
	y = v[0];

	do {
		e = (sum >> 2) & 3;
		for (p=n-1; p>0; p--) {
			z = v[p-1];
			y = v[p] -= MX;
		}

		z = v[n-1];
		y = v[0] -= MX;
		sum -= DELTA;
	} while (--rounds);
}

void nolo_decrypt_data(unsigned char* buf)
{
	//U_LOG_D("Start of function");
	static const uint32_t key[4] = {0x875bcc51, 0xa7637a66, 0x50960967, 0xf8536c51};
	uint32_t cryptpart[CRYPT_WORDS];

	// Decrypt encrypted portion
	for (int i = 0; i < CRYPT_WORDS; i++) {
	cryptpart[i] =
		((uint32_t)buf[CRYPT_OFFSET+4*i  ]) << 0  |
		((uint32_t)buf[CRYPT_OFFSET+4*i+1]) << 8  |
		((uint32_t)buf[CRYPT_OFFSET+4*i+2]) << 16 |
		((uint32_t)buf[CRYPT_OFFSET+4*i+3]) << 24;
	}

	btea_decrypt(cryptpart, CRYPT_WORDS, 1, key);

	for (int i = 0; i < CRYPT_WORDS; i++) {
		buf[CRYPT_OFFSET+4*i  ] = cryptpart[i] >> 0;
		buf[CRYPT_OFFSET+4*i+1] = cryptpart[i] >> 8;
		buf[CRYPT_OFFSET+4*i+2] = cryptpart[i] >> 16;
		buf[CRYPT_OFFSET+4*i+3] = cryptpart[i] >> 24;
	}
}

/**
 * Decodes the packet data for a controller.
 * According to Nolo's Official Driver Headers, the following is the packet format:
 * [ VersionID(1B) | Position(3B) | Rotation(4B) | Button Inputs(1B) | Touched(1B) | Touch Axis(2B) | Battery(1B) | State(1B) ]
*/
void nolo_decode_controller(struct nolo_device* device, const unsigned char* data)
{
	uint8_t bit; 
	uint8_t buttonstate;

	struct xrt_quat orientation;
	nolo_sample smp;

	device->version_id = read8(&data);

	device->pose.position.x = read16(&data)*POSITION_SCALE;
	device->pose.position.y = read16(&data)*POSITION_SCALE;
	device->pose.position.z = read16(&data)*POSITION_SCALE;

	device->raw_accel.x = read16(&data);
	device->raw_accel.z = -(float)read16(&data);
	device->raw_accel.y = read16(&data);

	device->raw_gyro.x = read16(&data);
	device->raw_gyro.z = -(float)read16(&data);
	device->raw_gyro.y = read16(&data);

	// Button State (1B) + Touched (1B) so 2Bs
	buttonstate   = read8(&data);
	for (bit=0; bit<6; bit++)
		device->controller_values[bit] = (buttonstate & 1<<bit ? 1 : 0);

	device->controller_values[6] = read8(&data); // X Pad
	device->controller_values[7] = read8(&data); // Y Pad

	device->battery              = read8(&data); 
	device->connected            = read8(&data);
	device->tick                 = read8(&data);

	//device->sample = smp; //Set sample for fusion

	/*
		// Note, header is either 0 or 1 and determines the packet for controller 0 or controller 1
		// HMD tracker data is present in all packets but controller data alternates
		// calculate w sqrt (1.0 - x^2 + y^2 + z^2)
		//pose.orientation.w = sqrt(1.0 - smp.rot[0]*smp.rot[0] + smp.rot[1]*smp.rot[1] + smp.rot[2]*smp.rot[2]);
	*/
	//if(device->base.device_type == XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER){
	NOLO_DEBUG_USB_CONTROLLER_PACKET(device,"[ %22s | %4d | P(%8g,%8g,%8g) | A(%8g,%8g,%8g) | G(%4g,%4g,%4g) | D(%4g) | I(%d,%4g,%4g) | C(%d) | B(%d) | T(%d)]",
		device->base.str,
		device->version_id,
		device->pose.position.x,device->pose.position.y,device->pose.position.z,
		device->raw_accel.x,device->raw_accel.y,device->raw_accel.z,
		device->raw_gyro.x,device->raw_gyro.y,device->raw_gyro.z,
		buttonstate,device->controller_values[6],device->controller_values[6],
		device->connected,
		device->battery,
		device->tick
	);
	//}
}

/**
 * Decode the HMD Part of the Packet
 * [ Version | Position | Init Position | 2 Point Drift Angle | Rotation | State ]
 * 
*/
void nolo_decode_hmd_marker(struct nolo_device* device, const unsigned char* data)
{
	struct xrt_vec3 homepos;
	struct xrt_vec3 init_position;
	struct xrt_quat orientation;
	nolo_sample smp;
	uint8_t state;
	uint8_t battery;

	float ipos_x,ipos_y,ipos_z;
	float v1,v2,v3;
	float zeros;

	data += 24; //Skip controller data

	device->version_id      = read8(&data);

	device->pose.position.x = read16(&data)*POSITION_SCALE;
	device->pose.position.y = read16(&data)*POSITION_SCALE;
	device->pose.position.z = read16(&data)*POSITION_SCALE;

	v1                      = read16(&data); //0
	v2                      = read16(&data); //0
	v3                      = read16(&data); //0

	device->raw_gyro.x      = read16(&data);
	device->raw_gyro.y      = read16(&data);
	device->raw_gyro.z      = -(float)read16(&data);


	device->home_position.x = read16(&data)*POSITION_SCALE;
	device->home_position.y = read16(&data)*POSITION_SCALE; 
	device->home_position.z = read16(&data)*POSITION_SCALE;

	device->raw_accel.x     = read16(&data); 
	device->raw_accel.y     = read16(&data); 
	device->raw_accel.z     = -(float)read16(&data);
	device->two_point_drift_angle = read16(&data);


	device->connected       = read8(&data);
	device->battery         = read8(&data);
	device->tick            = read8(&data);

	NOLO_DEBUG_USB_TRACKER_PACKET(device,"[ %22s | %4d | P(%8g,%8g,%8g) | A(%8g,%8g,%8g) | G(%4g,%4g,%4g) | i(%4g,%4g,%4g) | v(%4g,%4g,%4g) | D(%4g) | C(%d) | B(%d) T(%4ld)]",
		device->base.str,
		device->version_id,
		device->pose.position.x,device->pose.position.y,device->pose.position.z,
		device->raw_accel.x,device->raw_accel.y,device->raw_accel.z,
		device->raw_gyro.x,device->raw_gyro.y,device->raw_gyro.z,
		ipos_x,ipos_y,ipos_z,
		v1,v2,v3,
		device->two_point_drift_angle,
		device->connected,
		device->battery,
		device->tick
	);
}

void nolo_decode_base_station(struct nolo_device* priv, const unsigned char* data)
{
	// Unknown version
	if (data[0] != 2 || data[1] != 1)
		return;
}

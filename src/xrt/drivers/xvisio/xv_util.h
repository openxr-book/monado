#pragma once

#include "tracking/t_tracking.h"
#include "math/m_mathinclude.h"
#include "math/m_api.h"

#ifdef __cplusplus
extern "C" {
#endif

enum xr50_camera_id
{
	XR50_CAMERA_FRONT_LEFT = 0,
	XR50_CAMERA_FRONT_RIGHT = 1,
};

struct xr50_projection_pinhole
{
	float cx, cy; /* Principal point */
	float fx, fy; /* Focal length */
};

struct xr50_distortion_parameters
{
	double alpha, beta;
};

struct xr50_camera_calibration
{
	struct xrt_size image_size_pixels;
	struct xr50_projection_pinhole projection;
    struct xr50_distortion_parameters distortion;
};

struct xr50_camera_calibration_mono
{
    // The external coordinate (right-hand) system of all cameras is based on IMU as the origin
    // Device's 6dof center point is on IMU
	struct xrt_matrix_4x4 camera_from_imu;

    struct xr50_camera_calibration camera_calibration;
};

struct xr50_camera_calibration_stereo
{
	struct xr50_camera_calibration_mono cameras[2];
};

struct t_camera_calibration
xvisio_xr50_get_cam_calib(struct xr50_camera_calibration_stereo *camera_calibration_stereo, enum xr50_camera_id cam_id);

struct t_stereo_camera_calibration *
xvisio_xr50_create_stereo_camera_calib_rotated(struct xr50_camera_calibration_stereo *camera_calibration);

#ifdef __cplusplus
}
#endif
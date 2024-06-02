#include <stdbool.h>
#include <math.h>

#include "tinyceres/tiny_solver.hpp"
#include "tinyceres/tiny_solver_autodiff_function.hpp"

#include "xv_util.h"

using ceres::TinySolver;
using ceres::TinySolverAutoDiffFunction;

const int N_KB4_DISTORT_PARAMS = 4;

// https://hal.science/hal-01722264/document
// https://arxiv.org/pdf/1807.08957.pdf
// https://github.com/ethz-asl/image_undistort/blob/master/src/undistorter.cpp

template <typename T>
bool
eucm_undistort_func(struct t_camera_calibration *calib,
                         const double *distortion_params,
                         const T point[2],
                         T *out_point)
{
	const T x = point[0];
	const T y = point[1];
	const double z = 1;

	/*

	calib->intrinsic:

	fx	0	cx
	0	fy	cy
	0	0	0

	*/
	const double fx = calib->intrinsics[0][0];
	const double fy = calib->intrinsics[1][1];

	const double cx = calib->intrinsics[0][2];
	const double cy = calib->intrinsics[1][2];


	const double alpha = distortion_params[0];
	const double beta = distortion_params[1];

	const T delta = sqrt(beta * (x*x + y*y) + z*z);
	const T scaling = 1.0 / (alpha*delta + (1-alpha)*z);

	const T xd = x * scaling;
	const T yd = y * scaling;

	out_point[0] = fx * xd + cx;
	out_point[1] = fy * yd + cy;

	return true;
}

struct UndistortCostFunctor
{
	UndistortCostFunctor(struct t_camera_calibration *calib, double *distortion_params, double point[2])
	    : m_calib(calib), m_distortion_params(distortion_params)
	{
		m_point[0] = point[0];
		m_point[1] = point[1];
	}

	struct t_camera_calibration *m_calib;
	double *m_distortion_params;
	double m_point[2];

	template <typename T>
	bool
	operator()(const T *const x, T *residual) const
	{
		T out_point[2];

		if (!eucm_undistort_func(m_calib, m_distortion_params, x, out_point))
			return false;

		residual[0] = out_point[0] - m_point[0];
		residual[1] = out_point[1] - m_point[1];
		return true;
	}
};

template <typename T>
bool
kb4_distort_func(struct t_camera_calibration *calib, const T *distortion_params, const double point[2], T *out_point)
{
	const double x = point[0];
	const double y = point[1];

	const double r2 = x * x + y * y;
	const double r = sqrt(r2);

	const double fx = calib->intrinsics[0][0];
	const double fy = calib->intrinsics[1][1];

	const double cx = calib->intrinsics[0][2];
	const double cy = calib->intrinsics[1][2];

	if (r < 1e-8) {
		out_point[0] = T(fx * x + cx);
		out_point[1] = T(fy * y + cy);
		return true;
	}

	const double theta = atan(r);
	const double theta2 = theta * theta;

	const T k1 = distortion_params[0];
	const T k2 = distortion_params[1];
	const T k3 = distortion_params[2];
	const T k4 = distortion_params[3];

	T r_theta = k4 * theta2;

	r_theta += k3;
	r_theta *= theta2;
	r_theta += k2;
	r_theta *= theta2;
	r_theta += k1;
	r_theta *= theta2;
	r_theta += 1;
	r_theta *= theta;

	const T mx = x * r_theta / r;
	const T my = y * r_theta / r;

	out_point[0] = fx * mx + cx;
	out_point[1] = fy * my + cy;

	return true;
}

struct TargetPoint
{
	double point[2];
	double distorted[2];
};

struct DistortParamKB4CostFunctor
{
	DistortParamKB4CostFunctor(struct t_camera_calibration *calib, int nSteps, TargetPoint *targetPointGrid)
	    : m_calib(calib), m_nSteps(nSteps), m_targetPointGrid(targetPointGrid)
	{}

	struct t_camera_calibration *m_calib;
	int m_nSteps;
	TargetPoint *m_targetPointGrid;

	template <typename T>
	bool
	operator()(const T *const distort_params, T *residual) const
	{
		T out_point[2];

		for (int y_index = 0; y_index < m_nSteps; y_index++) {
			for (int x_index = 0; x_index < m_nSteps; x_index++) {
				int residual_index = 2 * (y_index * m_nSteps + x_index);
				TargetPoint *p = &m_targetPointGrid[(y_index * m_nSteps) + x_index];

				if (!kb4_distort_func<T>(m_calib, distort_params, p->point, out_point))
					return false;

				residual[residual_index + 0] = out_point[0] - p->distorted[0];
				residual[residual_index + 1] = out_point[1] - p->distorted[1];
			}
		}

		return true;
	}
};

#define STEPS 21
struct t_camera_calibration
xvisio_xr50_get_cam_calib(struct xr50_camera_calibration_stereo *camera_calibration_stereo, enum xr50_camera_id cam_id)
{
	struct t_camera_calibration tcc;

	struct xr50_camera_calibration_mono *xr50_cam = &camera_calibration_stereo->cameras[cam_id];

	tcc.image_size_pixels.h = xr50_cam->camera_calibration.image_size_pixels.h;
	tcc.image_size_pixels.w = xr50_cam->camera_calibration.image_size_pixels.w;

	tcc.intrinsics[0][0] = xr50_cam->camera_calibration.projection.fx;
	tcc.intrinsics[1][1] = xr50_cam->camera_calibration.projection.fy;
	tcc.intrinsics[0][2] = xr50_cam->camera_calibration.projection.cx;
	tcc.intrinsics[1][2] = xr50_cam->camera_calibration.projection.cy;
	tcc.intrinsics[2][2] = 1.0;
	tcc.distortion_model = T_DISTORTION_FISHEYE_KB4;

	TargetPoint xy[STEPS * STEPS];

	/* Convert EUCM params to KB4: */
	double eucm_distort_params[2];
	eucm_distort_params[0] = xr50_cam->camera_calibration.distortion.alpha;
	eucm_distort_params[1] = xr50_cam->camera_calibration.distortion.beta;

	/* Calculate EUCM distortion grid by finding the viewplane coordinates that
	 * project onto the points of grid spaced across the pixel image plane */
	for (int y_index = 0; y_index < STEPS; y_index++) {
		for (int x_index = 0; x_index < STEPS; x_index++) {
			int x = x_index * (tcc.image_size_pixels.w - 1) / (STEPS - 1);
			int y = y_index * (tcc.image_size_pixels.h - 1) / (STEPS - 1);
			TargetPoint *p = &xy[(y_index * STEPS) + x_index];

			p->distorted[0] = x;
			p->distorted[1] = y;

			Eigen::Matrix<double, 2, 1> result(0, 0);

			using AutoDiffUndistortFunction = TinySolverAutoDiffFunction<UndistortCostFunctor, 2, 2>;
			UndistortCostFunctor undistort_functor(&tcc, eucm_distort_params, p->distorted);
			AutoDiffUndistortFunction f(undistort_functor);

			TinySolver<AutoDiffUndistortFunction> solver;
			solver.Solve(f, &result);

			p->point[0] = result[0];
			p->point[1] = result[1];
		}
	}

	/* Use the calculated distortion grid to solve for kb4 params */
	{
		Eigen::Matrix<double, N_KB4_DISTORT_PARAMS, 1> kb4_distort_params;

		using AutoDiffDistortParamKB4Function =
		    TinySolverAutoDiffFunction<DistortParamKB4CostFunctor, 2 * STEPS * STEPS, N_KB4_DISTORT_PARAMS>;
		DistortParamKB4CostFunctor distort_param_kb4_functor(&tcc, STEPS, xy);
		AutoDiffDistortParamKB4Function f(distort_param_kb4_functor);

		TinySolver<AutoDiffDistortParamKB4Function> solver;
		solver.Solve(f, &kb4_distort_params);

		tcc.kb4.k1 = kb4_distort_params[0];
		tcc.kb4.k2 = kb4_distort_params[1];
		tcc.kb4.k3 = kb4_distort_params[2];
		tcc.kb4.k4 = kb4_distort_params[3];

		return tcc;
	}
}

struct t_stereo_camera_calibration *
xvisio_xr50_create_stereo_camera_calib_rotated(struct xr50_camera_calibration_stereo *camera_calibration_stereo)
{
	struct t_stereo_camera_calibration *calib = NULL;
	t_stereo_camera_calibration_alloc(&calib, T_DISTORTION_FISHEYE_KB4);

	struct xr50_camera_calibration_mono *left = &camera_calibration_stereo->cameras[XR50_CAMERA_FRONT_LEFT];
	struct xr50_camera_calibration_mono *right = &camera_calibration_stereo->cameras[XR50_CAMERA_FRONT_RIGHT];

	// intrinsics
	for (int view = 0; view < 2; view++) {
		enum xr50_camera_id cam_id = view == 0 ? XR50_CAMERA_FRONT_LEFT : XR50_CAMERA_FRONT_RIGHT;
		calib->view[view] = xvisio_xr50_get_cam_calib(camera_calibration_stereo, cam_id);
	}

	// calculate vector going from left camera to right camera
	// vector *from* imu to each camera is given by xr50 calibration
    struct xrt_pose left_from_imu, right_from_imu;
	// invert it
	struct xrt_pose imu_from_left;
	// left->imu->right = left->right = right_from_left
	struct xrt_pose right_from_left;
	struct xrt_matrix_3x3 right_from_left_rot;

	math_pose_from_isometry(&left->camera_from_imu, &left_from_imu);
	math_pose_from_isometry(&right->camera_from_imu, &right_from_imu);

	math_pose_invert(&left_from_imu, &imu_from_left);
	math_pose_transform(&imu_from_left, &right_from_imu, &right_from_left);
	math_matrix_3x3_from_quat(&right_from_left.orientation, &right_from_left_rot);

	calib->camera_translation[0] = right_from_left.position.x;
	calib->camera_translation[1] = right_from_left.position.y;
	calib->camera_translation[2] = right_from_left.position.z;

	calib->camera_rotation[0][0] = right_from_left_rot.v[0];
	calib->camera_rotation[0][1] = right_from_left_rot.v[1];
	calib->camera_rotation[0][2] = right_from_left_rot.v[2];
	calib->camera_rotation[1][0] = right_from_left_rot.v[3];
	calib->camera_rotation[1][1] = right_from_left_rot.v[4];
	calib->camera_rotation[1][2] = right_from_left_rot.v[5];
	calib->camera_rotation[2][0] = right_from_left_rot.v[6];
	calib->camera_rotation[2][1] = right_from_left_rot.v[7];
	calib->camera_rotation[2][2] = right_from_left_rot.v[8];

	return calib;
}
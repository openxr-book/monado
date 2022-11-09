// Copyright 2020-2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Internal C++ header for SDL XR system.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup sdl_test
 */

#pragma once

#ifndef __cplusplus
#error "This header is C++ only"
#endif

#include "sdl_internal.h"

#include "LandmarkCoreIncludes.h"
#include "GazeEstimation.h"

#include <SequenceCapture.h>
#include <Visualizer.h>
#include <VisualizationUtils.h>

struct sdl_program_plus : sdl_program
{
	// CPP only things
	LandmarkDetector::FaceModelParameters det_parameters;

	// The modules that are being used for tracking
	LandmarkDetector::CLNF face_model;
	Utilities::SequenceCapture sequence_reader;

	xrt_vec3 initialPoseEstimate;
	xrt_vec3 currentPoseEstimate;
	xrt_vec3 relativePoseEstimate;
};

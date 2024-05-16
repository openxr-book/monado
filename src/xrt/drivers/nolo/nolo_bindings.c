// Copyright 2022-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Bindings structs for @ref drv_nolo 
 * @author Michael Speth <mspeth@monky-games.com>
 * @ingroup drv_nolo
 */

#include "nolo_bindings.h"

#include "xrt/xrt_device.h"


/*
 *
 * Index Controller
 *
 */

struct xrt_binding_input_pair simple_inputs_nolo[9] = {
    {XRT_INPUT_TOUCH_TRIGGER_VALUE, XRT_INPUT_NOLO_TRIGGER_CLICK},
    {XRT_INPUT_SIMPLE_MENU_CLICK, XRT_INPUT_NOLO_MENU_CLICK},
    {XRT_INPUT_TOUCH_SYSTEM_CLICK, XRT_INPUT_NOLO_SYSTEM_CLICK},
    {XRT_INPUT_TOUCH_SQUEEZE_VALUE, XRT_INPUT_NOLO_SQUEEZE_CLICK},
    {XRT_INPUT_SIMPLE_GRIP_POSE, XRT_INPUT_NOLO_GRIP_POSE},
    {XRT_INPUT_SIMPLE_AIM_POSE, XRT_INPUT_NOLO_AIM_POSE},
    {XRT_INPUT_TOUCH_THUMBSTICK_TOUCH, XRT_INPUT_NOLO_TRACKPAD_TOUCH},
    {XRT_INPUT_TOUCH_THUMBSTICK_CLICK, XRT_INPUT_NOLO_TRACKPAD_CLICK},
    {XRT_INPUT_TOUCH_THUMBSTICK, XRT_INPUT_NOLO_TRACKPAD},
};

struct xrt_binding_output_pair simple_outputs_nolo[0] = {
};

struct xrt_binding_profile binding_profiles_nolo[1] = {
    {
        .name = XRT_DEVICE_SIMPLE_CONTROLLER,
        .inputs = simple_inputs_nolo,
        .input_count = ARRAY_SIZE(simple_inputs_nolo),
        .outputs = simple_outputs_nolo,
        .output_count = ARRAY_SIZE(simple_outputs_nolo),
    },
};

uint32_t binding_profiles_nolo_count = ARRAY_SIZE(binding_profiles_nolo);
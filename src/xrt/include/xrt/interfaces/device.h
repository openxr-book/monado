// Copyright 2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Interface header for xrt_device
 * @author Simon Zeni <simon.zeni@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt/xrt_defines.h"

struct xrt_device;
struct xrt_visibility_mask;

// TODO: move to its own file
struct xrt_device_interface
{
	/*!
	 * User friendly interface name, used for debugging and displaying
	 */
	const char *name;

	/*!
	 * @brief Destroy device and clean up all of its resources.
	 *
	 * Mandatory function for all type of device
	 *
	 * @param[in] xdev	Device to be destroyed.
	 */
	void (*destroy)(struct xrt_device *xdev);

	/*!
	 * Update any attached inputs. Returns `false` on failure.
	 *
	 * Optional function for all type of device
	 *
	 * @param[in] xdev        The device.
	 */
	bool (*update_inputs)(struct xrt_device *xdev);

	/*!
	 * @brief Get relationship of a tracked device to the tracking origin
	 * space as the base space.
	 *
	 * It is the responsibility of the device driver to do any prediction,
	 * there are helper functions available for this.
	 *
	 * The timestamps are system monotonic timestamps, such as returned by
	 * os_monotonic_get_ns().
	 *
	 * @param[in] xdev           The device.
	 * @param[in] name           Some devices may have multiple poses on
	 *                           them, select the one using this field. For
	 *                           HMDs use @p XRT_INPUT_GENERIC_HEAD_POSE.
	 *                           For Unbounded Reference Space you can use
	 *                           @p XRT_INPUT_GENERIC_UNBOUNDED_SPACE_POSE
	 *                           to get the origin of that space.
	 * @param[in] at_timestamp_ns If the device can predict or has a history
	 *                            of positions, this is when the caller
	 *                            wants the pose to be from.
	 * @param[out] out_relation The relation read from the device.
	 *
	 * @see xrt_input_name
	 */
	void (*get_tracked_pose)(struct xrt_device *xdev,
	                         enum xrt_input_name name,
	                         uint64_t at_timestamp_ns,
	                         struct xrt_space_relation *out_relation);

	/*!
	 * @brief Get relationship of hand joints to the tracking origin space as
	 * the base space.
	 *
	 * It is the responsibility of the device driver to either do prediction
	 * or return joints from a previous time and write that time out to
	 * @p out_timestamp_ns.
	 *
	 * The timestamps are system monotonic timestamps, such as returned by
	 * os_monotonic_get_ns().
	 *
	 * @param[in] xdev                 The device.
	 * @param[in] name                 Some devices may have multiple poses on
	 *                                 them, select the one using this field. For
	 *                                 hand tracking use @p XRT_INPUT_GENERIC_HAND_TRACKING_DEFAULT_SET.
	 * @param[in] desired_timestamp_ns If the device can predict or has a history
	 *                                 of positions, this is when the caller
	 *                                 wants the pose to be from.
	 * @param[out] out_value           The hand joint data read from the device.
	 * @param[out] out_timestamp_ns    The timestamp of the data being returned.
	 *
	 * @see xrt_input_name
	 */
	void (*get_hand_tracking)(struct xrt_device *xdev,
	                          enum xrt_input_name name,
	                          uint64_t desired_timestamp_ns,
	                          struct xrt_hand_joint_set *out_value,
	                          uint64_t *out_timestamp_ns);

	/*!
	 * @brief Get the requested blend shape properties & weights for a face tracker
	 *
	 * @param[in] xdev                    The device.
	 * @param[in] facial_expression_type  The facial expression data type (XR_FB_face_tracking,
	 * XR_HTC_facial_tracking, etc).
	 * @param[in] out_value               Set of requested expression weights & blend shape properties.
	 *
	 * @see xrt_input_name
	 */
	xrt_result_t (*get_face_tracking)(struct xrt_device *xdev,
	                                  enum xrt_input_name facial_expression_type,
	                                  struct xrt_facial_expression_set *out_value);

	/*!
	 * Set a output value.
	 *
	 * @param[in] xdev           The device.
	 * @param[in] name           The output component name to set.
	 * @param[in] value          The value to set the output to.
	 * @see xrt_output_name
	 */
	void (*set_output)(struct xrt_device *xdev, enum xrt_output_name name, const union xrt_output_value *value);

	/*!
	 * @brief Get the per-view pose in relation to the view space.
	 *
	 * On most devices with coplanar displays and no built-in eye tracking
	 * or IPD sensing, this just calls a helper to process the provided
	 * eye relation, but this may also handle canted displays as well as
	 * eye tracking.
	 *
	 * Incorporates a call to xrt_device::get_tracked_pose or a wrapper for it
	 *
	 * @param[in] xdev         The device.
	 * @param[in] default_eye_relation
	 *                         The interpupillary relation as a 3D position.
	 *                         Most simple stereo devices would just want to
	 *                         set `out_pose->position.[x|y|z] = ipd.[x|y|z]
	 *                         / 2.0f` and adjust for left vs right view.
	 *                         Not to be confused with IPD that is absolute
	 *                         distance, this is a full 3D translation
	 *                         If a device has a more accurate/dynamic way of
	 *                         knowing the eye relation, it may ignore this
	 *                         input.
	 * @param[in] at_timestamp_ns This is when the caller wants the poses and FOVs to be from.
	 * @param[in] view_count   Number of views.
	 * @param[out] out_head_relation
	 *                         The head pose in the device tracking space.
	 *                         Combine with @p out_poses to get the views in
	 *                         device tracking space.
	 * @param[out] out_fovs    An array (of size @p view_count ) to populate
	 *                         with the device-suggested fields of view.
	 * @param[out] out_poses   An array (of size @p view_count ) to populate
	 *                         with view output poses in head space. When
	 *                         implementing, be sure to also set orientation:
	 *                         most likely identity orientation unless you
	 *                         have canted screens.
	 *                         (Caution: Even if you have eye tracking, you
	 *                         won't use eye orientation here!)
	 */
	void (*get_view_poses)(struct xrt_device *xdev,
	                       const struct xrt_vec3 *default_eye_relation,
	                       uint64_t at_timestamp_ns,
	                       uint32_t view_count,
	                       struct xrt_space_relation *out_head_relation,
	                       struct xrt_fov *out_fovs,
	                       struct xrt_pose *out_poses);
	/**
	 * Compute the distortion at a single point.
	 *
	 * The input is @p u @p v in screen/output space (that is, predistorted), you are to compute and return the u,v
	 * coordinates to sample the render texture. The compositor will step through a range of u,v parameters to build
	 * the lookup (vertex attribute or distortion texture) used to pre-distort the image as required by the device's
	 * optics.
	 *
	 * @param xdev            the device
	 * @param view            the view index
	 * @param u               horizontal texture coordinate
	 * @param v               vertical texture coordinate
	 * @param[out] out_result corresponding u,v pairs for all three color channels.
	 */
	bool (*compute_distortion)(
	    struct xrt_device *xdev, uint32_t view, float u, float v, struct xrt_uv_triplet *out_result);

	/*!
	 * Get the visibility mask for this device.
	 *
	 * @param[in] xdev       The device.
	 * @param[in] type       The type of visibility mask.
	 * @param[in] view_index The index of the view to get the mask for.
	 * @param[out] out_mask  Output mask, caller must free.
	 */
	xrt_result_t (*get_visibility_mask)(struct xrt_device *xdev,
	                                    enum xrt_visibility_mask_type type,
	                                    uint32_t view_index,
	                                    struct xrt_visibility_mask **out_mask);

	/*!
	 * Called by the @ref xrt_space_overseer when a reference space that is
	 * implemented by this device is first used, or when the last usage of
	 * the reference space stops.
	 *
	 * What is provided is both the @ref xrt_reference_space_type that
	 * triggered the usage change and the @ref xrt_input_name (if any) that
	 * is used to drive the space.
	 *
	 * @see xrt_space_overseer_ref_space_inc
	 * @see xrt_space_overseer_ref_space_dec
	 * @see xrt_input_name
	 * @see xrt_reference_space_type
	 */
	xrt_result_t (*ref_space_usage)(struct xrt_device *xdev,
	                                enum xrt_reference_space_type type,
	                                enum xrt_input_name name,
	                                bool used);

	/*!
	 * @brief Check if given form factor is available or not.
	 *
	 * This should only be used in HMD device, if the device driver supports form factor check.
	 *
	 * @param[in] xdev The device.
	 * @param[in] form_factor Form factor to check.
	 *
	 * @return true if given form factor is available; otherwise false.
	 */
	bool (*is_form_factor_available)(struct xrt_device *xdev, enum xrt_form_factor form_factor);
};

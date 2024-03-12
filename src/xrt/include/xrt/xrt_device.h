// Copyright 2019-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Header defining an xrt display or controller device.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Moses Turner <mosesturner@protonmail.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @author Simon Zeni <simon.zeni@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt/interfaces/device.h"
#include "xrt/xrt_defines.h"
#include "xrt/xrt_visibility_mask.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xrt_device;
struct xrt_tracking;

#define XRT_DEVICE_NAME_LEN 256

/*!
 * A per-lens/display view information.
 *
 * @ingroup xrt_iface
 */
struct xrt_view
{
	/*!
	 * @brief Viewport position on the screen.
	 *
	 * In absolute screen coordinates on an unrotated display, like the
	 * HMD presents it to the OS.
	 *
	 * This field is only used by @ref comp to setup the device rendering.
	 *
	 * If the view is being rotated by xrt_view.rot 90° right in the
	 * distortion shader then `display.w_pixels == viewport.h_pixels` and
	 * `display.h_pixels == viewport.w_pixels`.
	 */
	struct
	{
		uint32_t x_pixels;
		uint32_t y_pixels;
		uint32_t w_pixels;
		uint32_t h_pixels;
	} viewport;

	/*!
	 * @brief Physical properties of this display (or the part of a display
	 * that covers this view).
	 *
	 * Not in absolute screen coordinates but like the clients see them i.e.
	 * after rotation is applied by xrt_view::rot.
	 * This field is only used for the clients' swapchain setup.
	 *
	 * The xrt_view::display::w_pixels and xrt_view::display::h_pixels
	 * become the recommended image size for this view, after being scaled
	 * by the debug environment variable `XRT_COMPOSITOR_SCALE_PERCENTAGE`.
	 */
	struct
	{
		uint32_t w_pixels;
		uint32_t h_pixels;
	} display;

	/*!
	 * @brief Rotation 2d matrix used to rotate the position of the output
	 * of the distortion shaders onto the screen.
	 *
	 * If the distortion shader is based on a mesh, then this matrix rotates
	 * the vertex positions.
	 */
	struct xrt_matrix_2x2 rot;
};

/*!
 * All of the device components that deals with interfacing to a users head.
 *
 * HMD is probably a bad name for the future but for now will have to do.
 *
 * @ingroup xrt_iface
 */
struct xrt_hmd_parts
{
	/*!
	 * @brief The hmd screen as an unrotated display, like the HMD presents
	 * it to the OS.
	 *
	 * This field is used by @ref comp to setup the extended mode window.
	 */
	struct
	{
		int w_pixels;
		int h_pixels;
		//! Nominal frame interval
		uint64_t nominal_frame_interval_ns;
	} screens[1];

	/*!
	 * Display information.
	 *
	 * For now hardcoded display to two.
	 */
	struct xrt_view views[2];

	/*!
	 * Array of supported blend modes.
	 */
	enum xrt_blend_mode blend_modes[XRT_MAX_DEVICE_BLEND_MODES];
	size_t blend_mode_count;

	/*!
	 * Distortion information.
	 */
	struct
	{
		//! Supported distortion models, a bitfield.
		enum xrt_distortion_model models;
		//! Preferred disortion model, single value.
		enum xrt_distortion_model preferred;

		struct
		{
			//! Data.
			float *vertices;
			//! Number of vertices.
			uint32_t vertex_count;
			//! Stride of vertices
			uint32_t stride;
			//! 1 or 3 for (chromatic aberration).
			uint32_t uv_channels_count;

			//! Indices, for triangle strip.
			int *indices;
			//! Number of indices for the triangle strips (one per view).
			uint32_t index_counts[2];
			//! Offsets for the indices (one offset per view).
			uint32_t index_offsets[2];
			//! Total number of elements in mesh::indices array.
			uint32_t index_count_total;
		} mesh;

		//! distortion is subject to the field of view
		struct xrt_fov fov[2];
	} distortion;
};

/*!
 * A single named input, that sits on a @ref xrt_device.
 *
 * @ingroup xrt_iface
 */
struct xrt_input
{
	//! Is this input active.
	bool active;

	int64_t timestamp;

	enum xrt_input_name name;

	union xrt_input_value value;
};

/*!
 * A single named output, that sits on a @ref xrt_device.
 *
 * @ingroup xrt_iface
 */
struct xrt_output
{
	enum xrt_output_name name;
};


/*!
 * A binding pair, going @p from a binding point to a @p device input.
 *
 * @ingroup xrt_iface
 */
struct xrt_binding_input_pair
{
	enum xrt_input_name from;   //!< From which name.
	enum xrt_input_name device; //!< To input on the device.
};

/*!
 * A binding pair, going @p from a binding point to a @p device output.
 *
 * @ingroup xrt_iface
 */
struct xrt_binding_output_pair
{
	enum xrt_output_name from;   //!< From which name.
	enum xrt_output_name device; //!< To output on the device.
};

/*!
 * A binding profile, has lists of binding pairs to goes from device in @p name
 * to the device it hangs off on.
 *
 * @ingroup xrt_iface
 */
struct xrt_binding_profile
{
	//! Device this binding emulates.
	enum xrt_device_name name;

	struct xrt_binding_input_pair *inputs;
	size_t input_count;
	struct xrt_binding_output_pair *outputs;
	size_t output_count;
};

/*!
 * @interface xrt_device
 *
 * A single HMD or input device.
 *
 * @ingroup xrt_iface
 */
struct xrt_device
{
	//! Device interface implementation
	const struct xrt_device_interface *impl;

	//! Enum identifier of the device.
	enum xrt_device_name name;
	enum xrt_device_type device_type;

	//! A string describing the device.
	char str[XRT_DEVICE_NAME_LEN];

	//! A unique identifier. Persistent across configurations, if possible.
	char serial[XRT_DEVICE_NAME_LEN];

	//! Null if this device does not interface with the users head.
	struct xrt_hmd_parts *hmd;

	//! Always set, pointing to the tracking system for this device.
	struct xrt_tracking_origin *tracking_origin;

	//! Number of bindings in xrt_device::binding_profiles
	size_t binding_profile_count;
	// Array of alternative binding profiles.
	struct xrt_binding_profile *binding_profiles;

	//! Number of inputs.
	size_t input_count;
	//! Array of input structs.
	struct xrt_input *inputs;

	//! Number of outputs.
	size_t output_count;
	//! Array of output structs.
	struct xrt_output *outputs;

	bool orientation_tracking_supported;
	bool position_tracking_supported;
	bool hand_tracking_supported;
	bool eye_gaze_supported;
	bool force_feedback_supported;
	bool ref_space_usage_supported;
	bool form_factor_check_supported;
	bool stage_supported;
	bool face_tracking_supported;
};

/*!
 * Helper function for @ref xrt_device::update_inputs.
 *
 * @copydoc xrt_device::update_inputs
 *
 * @public @memberof xrt_device
 */
static inline bool
xrt_device_update_inputs(struct xrt_device *xdev)
{
	if (xdev->impl->update_inputs) {
		return xdev->impl->update_inputs(xdev);
	}

	return true;
}

/*!
 * Helper function for @ref xrt_device::get_tracked_pose.
 *
 * @copydoc xrt_device::get_tracked_pose
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_get_tracked_pose(struct xrt_device *xdev,
                            enum xrt_input_name name,
                            uint64_t at_timestamp_ns,
                            struct xrt_space_relation *out_relation)
{
	xdev->impl->get_tracked_pose(xdev, name, at_timestamp_ns, out_relation);
}

/*!
 * Helper function for @ref xrt_device::get_hand_tracking.
 *
 * @copydoc xrt_device::get_hand_tracking
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_get_hand_tracking(struct xrt_device *xdev,
                             enum xrt_input_name name,
                             uint64_t desired_timestamp_ns,
                             struct xrt_hand_joint_set *out_value,
                             uint64_t *out_timestamp_ns)
{
	xdev->impl->get_hand_tracking(xdev, name, desired_timestamp_ns, out_value, out_timestamp_ns);
}

/*!
 * Helper function for @ref xrt_device::get_face_tracking.
 *
 * @copydoc xrt_device::get_face_tracking
 *
 * @public @memberof xrt_device
 */
static inline xrt_result_t
xrt_device_get_face_tracking(struct xrt_device *xdev,
                             enum xrt_input_name facial_expression_type,
                             struct xrt_facial_expression_set *out_value)
{
	return xdev->impl->get_face_tracking(xdev, facial_expression_type, out_value);
}

/*!
 * Helper function for @ref xrt_device::set_output.
 *
 * @copydoc xrt_device::set_output
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_set_output(struct xrt_device *xdev, enum xrt_output_name name, const union xrt_output_value *value)
{
	xdev->impl->set_output(xdev, name, value);
}

/*!
 * Helper function for @ref xrt_device::get_view_poses.
 *
 * @copydoc xrt_device::get_view_poses
 * @public @memberof xrt_device
 */
static inline void
xrt_device_get_view_poses(struct xrt_device *xdev,
                          const struct xrt_vec3 *default_eye_relation,
                          uint64_t at_timestamp_ns,
                          uint32_t view_count,
                          struct xrt_space_relation *out_head_relation,
                          struct xrt_fov *out_fovs,
                          struct xrt_pose *out_poses)
{
	xdev->impl->get_view_poses(xdev, default_eye_relation, at_timestamp_ns, view_count, out_head_relation, out_fovs,
	                           out_poses);
}

/*!
 * Helper function for @ref xrt_device::compute_distortion.
 *
 * @copydoc xrt_device::compute_distortion
 *
 * @public @memberof xrt_device
 */
static inline bool
xrt_device_compute_distortion(
    struct xrt_device *xdev, uint32_t view, float u, float v, struct xrt_uv_triplet *out_result)
{
	return xdev->impl->compute_distortion(xdev, view, u, v, out_result);
}

/*!
 * Helper function for @ref xrt_device::get_visibility_mask.
 *
 * @copydoc xrt_device::get_visibility_mask
 *
 * @public @memberof xrt_device
 */
static inline xrt_result_t
xrt_device_get_visibility_mask(struct xrt_device *xdev,
                               enum xrt_visibility_mask_type type,
                               uint32_t view_index,
                               struct xrt_visibility_mask **out_mask)
{
	return xdev->impl->get_visibility_mask(xdev, type, view_index, out_mask);
}

/*!
 * Helper function for @ref xrt_device::ref_space_usage.
 *
 * @copydoc xrt_device::ref_space_usage
 *
 * @public @memberof xrt_device
 */
static inline xrt_result_t
xrt_device_ref_space_usage(struct xrt_device *xdev,
                           enum xrt_reference_space_type type,
                           enum xrt_input_name name,
                           bool used)
{
	return xdev->impl->ref_space_usage(xdev, type, name, used);
}

/*!
 * Helper function for @ref xrt_device::is_form_factor_available.
 *
 * @copydoc xrt_device::is_form_factor_available
 *
 * @public @memberof xrt_device
 */
static inline bool
xrt_device_is_form_factor_available(struct xrt_device *xdev, enum xrt_form_factor form_factor)
{
	return xdev->impl->is_form_factor_available(xdev, form_factor);
}

/*!
 * Helper function for @ref xrt_device::destroy.
 *
 * Handles nulls, sets your pointer to null.
 *
 * @public @memberof xrt_device
 */
static inline void
xrt_device_destroy(struct xrt_device **xdev_ptr)
{
	struct xrt_device *xdev = *xdev_ptr;
	if (xdev == NULL) {
		return;
	}

	xdev->impl->destroy(xdev);
	*xdev_ptr = NULL;
}


#ifdef __cplusplus
} // extern "C"
#endif

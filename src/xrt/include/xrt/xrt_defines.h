// Copyright 2019-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Common defines and enums for XRT.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Moses Turner <mosesturner@protonmail.com>
 * @author Nis Madsen <nima_zero_one@protonmail.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt/xrt_compiler.h"

#include "xrt/xrt_results.h"

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Internal define for VK_UUID_SIZE and XR_UUID_SIZE_EXT.
 *
 * @ingroup xrt_iface
 */
#define XRT_UUID_SIZE 16

/*!
 * To transport UUIDs between different APIs.
 *
 * @ingroup xrt_iface
 */
struct xrt_uuid
{
	uint8_t data[XRT_UUID_SIZE];
};

/*!
 * Typedef for @ref xrt_uuid.
 *
 * @ingroup xrt_iface
 */
typedef struct xrt_uuid xrt_uuid_t;

/*!
 * Internal define for VK_LUID_SIZE.
 *
 * @ingroup xrt_iface
 */
#define XRT_LUID_SIZE 8

/*!
 * To transport LUIDs between different APIs.
 *
 * @ingroup xrt_iface
 */
struct xrt_luid
{
	uint8_t data[XRT_LUID_SIZE];
};

/*!
 * Typedef for @ref xrt_luid.
 *
 * @ingroup xrt_iface
 */
typedef struct xrt_luid xrt_luid_t;

/*!
 * A limited unique id, it is only unique for the process it is in, so must not be
 * used or synchronized across process boundaries. A value of zero is invalid
 * and means it has not be properly initialised.
 *
 * @ingroup xrt_iface
 */
struct xrt_limited_unique_id
{
	uint64_t data;
};

/*!
 * Typedef for @ref xrt_limited_unique_id.
 *
 * @ingroup xrt_iface
 */
typedef struct xrt_limited_unique_id xrt_limited_unique_id_t;

/*!
 * A base class for reference counted objects.
 *
 * @ingroup xrt_iface
 */
struct xrt_reference
{
	xrt_atomic_s32_t count;
};

/*!
 * Blend mode that the device supports, exact mirror of XrEnvironmentBlendMode.
 *
 * This is not a bitmask because we want to be able to express a preference order
 * that may vary by device, etc.
 *
 * @ingroup xrt_iface
 */
enum xrt_blend_mode
{
	XRT_BLEND_MODE_OPAQUE = 1,
	XRT_BLEND_MODE_ADDITIVE = 2,
	XRT_BLEND_MODE_ALPHA_BLEND = 3,
	XRT_BLEND_MODE_MAX_ENUM,
};

#define XRT_MAX_DEVICE_BLEND_MODES 3

/*!
 * Special flags for creating passthrough.
 */
enum xrt_passthrough_create_flags
{
	//! Start the passthrough on creation
	XRT_PASSTHROUGH_IS_RUNNING_AT_CREATION = (1 << 0),
	//! Our compositor just ignores this bit.
	XRT_PASSTHROUGH_LAYER_DEPTH = (1 << 1),
};

/*!
 * Specify additional state change behavior.
 */
enum xrt_passthrough_state
{
	//! Passthrough system requires reinitialization.
	XRT_PASSTHROUGH_STATE_CHANGED_REINIT_REQUIRED_BIT = (1 << 0),
	//! Non-recoverable error has occurred.
	XRT_PASSTHROUGH_STATE_CHANGED_NON_RECOVERABLE_ERROR_BIT = (1 << 1),
	//! A recoverable error has occurred.
	XRT_PASSTHROUGH_STATE_CHANGED_RECOVERABLE_ERROR_BIT = (1 << 2),
	//! The runtime has recovered from a previous error and is functioning normally.
	XRT_PASSTHROUGH_STATE_CHANGED_RESTORED_ERROR_BIT = (1 << 3),
};

/*!
 * Specify the kind of passthrough behavior the layer provides.
 */
enum xrt_passthrough_purpose_flags
{
	//! Fullscreen layer
	XRT_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION = (1 << 0),
	//! Projected layer.
	XRT_PASSTHROUGH_LAYER_PURPOSE_PROJECTED = (1 << 1),
	//! Provided by XR_FB_passthrough_keyboard_hands
	XRT_PASSTHROUGH_LAYER_PURPOSE_TRACKED_KEYBOARD_HANDS = 1000203001,
	//! Provided by XR_FB_passthrough_keyboard_hands
	XRT_PASSTHROUGH_LAYER_PURPOSE_TRACKED_KEYBOARD_MASKED_HANDS = 1000203002,
};

/*!
 * Which distortion model does the device expose,
 * used both as a bitfield and value.
 */
enum xrt_distortion_model
{
	// clang-format off
	XRT_DISTORTION_MODEL_NONE      = 1u << 0u,
	XRT_DISTORTION_MODEL_COMPUTE   = 1u << 1u,
	XRT_DISTORTION_MODEL_MESHUV    = 1u << 2u,
	// clang-format on
};

/*!
 * Common formats, use `u_format_*` functions to reason about them.
 */
enum xrt_format
{
	XRT_FORMAT_R8G8B8X8,
	XRT_FORMAT_R8G8B8A8,
	XRT_FORMAT_R8G8B8,
	XRT_FORMAT_R8G8,
	XRT_FORMAT_R8,

	XRT_FORMAT_BAYER_GR8,

	XRT_FORMAT_L8, // Luminence, R = L, G = L, B = L.

	XRT_FORMAT_BITMAP_8X1, // One bit format tiled in 8x1 blocks.
	XRT_FORMAT_BITMAP_8X8, // One bit format tiled in 8X8 blocks.

	XRT_FORMAT_YUV888,
	XRT_FORMAT_YUYV422,
	XRT_FORMAT_UYVY422,

	XRT_FORMAT_MJPEG,
};

/*!
 * What type of stereo format a frame has.
 *
 * @ingroup xrt_iface
 */
enum xrt_stereo_format
{
	XRT_STEREO_FORMAT_NONE,
	XRT_STEREO_FORMAT_SBS,         //!< Side by side.
	XRT_STEREO_FORMAT_INTERLEAVED, //!< Interleaved pixels.
	XRT_STEREO_FORMAT_OAU,         //!< Over & Under.
};

/*!
 * A quaternion with single floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_quat
{
	float x;
	float y;
	float z;
	float w;
};

/*!
 * Identity value for @ref xrt_quat
 *
 * @ingroup xrt_iface math
 * @relates xrt_quat
 */
#define XRT_QUAT_IDENTITY                                                                                              \
	{                                                                                                              \
		0.f, 0.f, 0.f, 1.f                                                                                     \
	}

/*!
 * A 1 element vector with single floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_vec1
{
	float x;
};

/*!
 * A 2 element vector with single floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_vec2
{
	float x;
	float y;
};

/*!
 * Represents a uv triplet for distortion, basically just three xrt_vec2.
 *
 * @ingroup xrt_iface_math
 */
struct xrt_uv_triplet
{
	struct xrt_vec2 r, g, b;
};

/*!
 * A 3 element vector with single floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_vec3
{
	float x;
	float y;
	float z;
};

/*!
 * A 3 element vector with single doubles.
 *
 * @ingroup xrt_iface math
 */
struct xrt_vec3_f64
{
	double x;
	double y;
	double z;
};

/*!
 * All-zero value for @ref xrt_vec3
 *
 * @ingroup xrt_iface math
 * @relates xrt_vec3
 */
#define XRT_VEC3_ZERO                                                                                                  \
	{                                                                                                              \
		0.f, 0.f, 0.f                                                                                          \
	}
/*!
 * Value for @ref xrt_vec3 with 1 in the @p x coordinate.
 *
 * @ingroup xrt_iface math
 * @relates xrt_vec3
 */
#define XRT_VEC3_UNIT_X                                                                                                \
	{                                                                                                              \
		1.f, 0.f, 0.f                                                                                          \
	}
/*!
 * Value for @ref xrt_vec3 with 1 in the @p y coordinate.
 *
 * @ingroup xrt_iface math
 * @relates xrt_vec3
 */
#define XRT_VEC3_UNIT_Y                                                                                                \
	{                                                                                                              \
		0.f, 1.f, 0.f                                                                                          \
	}
/*!
 * Value for @ref xrt_vec3 with 1 in the @p z coordinate.
 *
 * @ingroup xrt_iface math
 * @relates xrt_vec3
 */
#define XRT_VEC3_UNIT_Z                                                                                                \
	{                                                                                                              \
		0.f, 0.f, 1.f                                                                                          \
	}

/*!
 * A 3 element vector with 32 bit integers.
 *
 * @ingroup xrt_iface math
 */
struct xrt_vec3_i32
{
	int32_t x;
	int32_t y;
	int32_t z;
};

/*!
 * A 2 element vector with 32 bit integers.
 *
 * @ingroup xrt_iface math
 */
struct xrt_vec2_i32
{
	int32_t x;
	int32_t y;
};

/*!
 * A 3 element colour with 8 bits per channel.
 *
 * @ingroup xrt_iface math
 */
struct xrt_colour_rgb_u8
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

/*!
 * A 4 element colour with 8 bits per channel.
 *
 * @ingroup xrt_iface math
 */
struct xrt_colour_rgba_u8
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

/*!
 * A 3 element colour with floating point channels.
 *
 * @ingroup xrt_iface math
 */
struct xrt_colour_rgb_f32
{
	float r;
	float g;
	float b;
};

/*!
 * A 4 element colour with floating point channels.
 *
 * @ingroup xrt_iface math
 */
struct xrt_colour_rgba_f32
{
	float r;
	float g;
	float b;
	float a;
};

/*!
 * Image size.
 *
 * @ingroup xrt_iface math
 */
struct xrt_size
{
	int w;
	int h;
};

/*!
 * Image offset.
 *
 * @ingroup xrt_iface math
 */
struct xrt_offset
{
	int w, h;
};

/*!
 * Image rectangle.
 *
 * @ingroup xrt_iface math
 */
struct xrt_rect
{
	struct xrt_offset offset;
	struct xrt_size extent;
};

/*!
 * Image rectangle
 *
 * @todo Unify xrt_rect and xrt_rect_f32 field names
 *
 * @ingroup xrt_iface math
 */
struct xrt_rect_f32
{
	float x, y, w, h;
};

/*!
 * Normalized image rectangle, coordinates and size in 0 .. 1 range.
 *
 * @ingroup xrt_iface math
 */
struct xrt_normalized_rect
{
	float x, y, w, h;
};

/*!
 * A pose composed of a position and orientation.
 *
 * @see xrt_qaut
 * @see xrt_vec3
 * @ingroup xrt_iface math
 */
struct xrt_pose
{
	struct xrt_quat orientation;
	struct xrt_vec3 position;
};
/*!
 * Identity value for @ref xrt_pose
 *
 * @ingroup xrt_iface math
 * @relates xrt_pose
 */
#define XRT_POSE_IDENTITY                                                                                              \
	{                                                                                                              \
		XRT_QUAT_IDENTITY, XRT_VEC3_ZERO                                                                       \
	}

/*!
 * Describes a projection matrix fov.
 *
 * @ingroup xrt_iface math
 */
struct xrt_fov
{
	float angle_left;
	float angle_right;
	float angle_up;
	float angle_down;
};

/*!
 * The number of values in @ref xrt_matrix_2x2
 *
 * @ingroup xrt_iface math
 */
#define XRT_MATRIX_2X2_ELEMENTS 4

/*!
 * The number of 2d vectors in @ref xrt_matrix_2x2
 *
 * @ingroup xrt_iface math
 */
#define XRT_MATRIX_2X2_VECS 2

/*!
 * A tightly packed 2x2 matrix of floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_matrix_2x2
{
	union {
		float v[XRT_MATRIX_2X2_ELEMENTS];
		struct xrt_vec2 vecs[XRT_MATRIX_2X2_VECS];
	};
};

/*!
 * The number of values in @ref xrt_matrix_3x3
 *
 * @ingroup xrt_iface math
 */
#define XRT_MATRIX_3X3_ELEMENTS 9

/*!
 * A tightly packed 3x3 matrix of floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_matrix_3x3
{
	float v[XRT_MATRIX_3X3_ELEMENTS];
};

/*!
 * A tightly packed 3x3 matrix of doubles.
 *
 * @ingroup xrt_iface math
 */
struct xrt_matrix_3x3_f64
{
	double v[XRT_MATRIX_3X3_ELEMENTS];
};

/*!
 * The number of values in a 4x4 matrix like @ref xrt_matrix_4x4 and @ref xrt_matrix_4x4_f64
 *
 * @ingroup xrt_iface math
 */
#define XRT_MATRIX_4X4_ELEMENTS 16

/*!
 * A tightly packed 4x4 matrix of floats.
 *
 * @ingroup xrt_iface math
 */
struct xrt_matrix_4x4
{
	float v[XRT_MATRIX_4X4_ELEMENTS];
};

/*!
 * A tightly packed 4x4 matrix of double.
 *
 * @ingroup xrt_iface math
 */
struct xrt_matrix_4x4_f64
{
	double v[XRT_MATRIX_4X4_ELEMENTS];
};

/*!
 * A range of API versions supported.
 *
 * @ingroup xrt_iface math
 */
struct xrt_api_requirements
{
	uint32_t min_major;
	uint32_t min_minor;
	uint32_t min_patch;

	uint32_t max_major;
	uint32_t max_minor;
	uint32_t max_patch;
};

/*!
 * Type of a OpenXR mapped reference space, maps to the semantic spaces on the
 * @ref xrt_space_overseer struct. This is used to refer to indirectly for
 * instance when letting the overseer know that an application is using a
 * particular reference space.
 *
 * @ingroup xrt_iface
 */
enum xrt_reference_space_type
{
	XRT_SPACE_REFERENCE_TYPE_VIEW,
	XRT_SPACE_REFERENCE_TYPE_LOCAL,
	XRT_SPACE_REFERENCE_TYPE_LOCAL_FLOOR,
	XRT_SPACE_REFERENCE_TYPE_STAGE,
	XRT_SPACE_REFERENCE_TYPE_UNBOUNDED,
};

/*!
 * The number of enumerations in @ref xrt_reference_space_type.
 *
 * @ingroup xrt_iface
 */
#define XRT_SPACE_REFERENCE_TYPE_COUNT (XRT_SPACE_REFERENCE_TYPE_UNBOUNDED + 1)

/*!
 * An invalid @ref xrt_reference_space_type, since it's invalid it's not listed
 * in the enum.
 *
 * @ingroup xrt_iface
 */
#define XRT_SPACE_REFERENCE_TYPE_INVALID ((enum xrt_reference_space_type)(-1))

/*!
 * Flags of which components of a @ref xrt_space_relation is valid.
 *
 * @see xrt_space_relation
 * @ingroup xrt_iface math
 */
enum xrt_space_relation_flags
{
	// clang-format off
	XRT_SPACE_RELATION_ORIENTATION_VALID_BIT =          (1u << 0u),
	XRT_SPACE_RELATION_POSITION_VALID_BIT =             (1u << 1u),
	XRT_SPACE_RELATION_LINEAR_VELOCITY_VALID_BIT =      (1u << 2u),
	XRT_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT =     (1u << 3u),
	XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT =        (1u << 4u),
	XRT_SPACE_RELATION_POSITION_TRACKED_BIT =           (1u << 5u),
	// clang-format on
	XRT_SPACE_RELATION_BITMASK_ALL = (uint32_t)XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |      //
	                                 (uint32_t)XRT_SPACE_RELATION_POSITION_VALID_BIT |         //
	                                 (uint32_t)XRT_SPACE_RELATION_LINEAR_VELOCITY_VALID_BIT |  //
	                                 (uint32_t)XRT_SPACE_RELATION_ANGULAR_VELOCITY_VALID_BIT | //
	                                 (uint32_t)XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT |    //
	                                 (uint32_t)XRT_SPACE_RELATION_POSITION_TRACKED_BIT,
	XRT_SPACE_RELATION_BITMASK_NONE = 0,
};

/*!
 * A relation with two spaces, includes velocity and acceleration.
 *
 * @see xrt_quat
 * @see xrt_vec3
 * @see xrt_pose
 * @see xrt_space_relation_flags
 * @ingroup xrt_iface math
 */
struct xrt_space_relation
{
	enum xrt_space_relation_flags relation_flags;
	struct xrt_pose pose;
	struct xrt_vec3 linear_velocity;
	struct xrt_vec3 angular_velocity;
};

/*!
 * A zero/identity value for @ref xrt_space_relation
 *
 * @note Despite this initializing all members (to zero or identity), this sets the xrt_space_relation::relation_flags
 * to XRT_SPACE_RELATION_BITMASK_NONE - so this is safe to assign before an error return, etc.
 *
 * @ingroup xrt_iface math
 * @relates xrt_space_relation
 */
#define XRT_SPACE_RELATION_ZERO                                                                                        \
	{                                                                                                              \
		XRT_SPACE_RELATION_BITMASK_NONE, XRT_POSE_IDENTITY, XRT_VEC3_ZERO, XRT_VEC3_ZERO                       \
	}

/*!
 * The maximum number of steps that can be in a relation chain.
 *
 * @see xrt_relation_chain::steps
 * @relates xrt_relation_chain
 * @ingroup xrt_iface math
 */
#define XRT_RELATION_CHAIN_CAPACITY 8

/*!
 * A chain of space relations and their associated validity flags.
 * Functions for manipulating this are available in `math/m_space.h`.
 *
 * @see xrt_space_relation
 * @ingroup xrt_iface math
 */
struct xrt_relation_chain
{
	struct xrt_space_relation steps[XRT_RELATION_CHAIN_CAPACITY];
	uint32_t step_count;
};


/*
 *
 * Input related enums and structs.
 *
 */

/*!
 * A enum that is used to name devices so that the
 * state trackers can reason about the devices easier.
 */
enum xrt_device_name
{
	XRT_DEVICE_INVALID = 0,

	XRT_DEVICE_GENERIC_HMD = 1,

	// Vive stuff.
	XRT_DEVICE_VIVE_PRO,
	XRT_DEVICE_VIVE_WAND,
	XRT_DEVICE_VIVE_TRACKER, // Generic, only used for bindings.
	XRT_DEVICE_VIVE_TRACKER_GEN1,
	XRT_DEVICE_VIVE_TRACKER_GEN2,
	XRT_DEVICE_VIVE_TRACKER_GEN3,
	XRT_DEVICE_VIVE_TRACKER_TUNDRA,

	// "Controllers" somewhat sorted as listed in spec.
	XRT_DEVICE_SIMPLE_CONTROLLER,
	XRT_DEVICE_DAYDREAM,
	XRT_DEVICE_WMR_CONTROLLER,
	XRT_DEVICE_XBOX_CONTROLLER,
	XRT_DEVICE_GO_CONTROLLER,
	XRT_DEVICE_TOUCH_CONTROLLER,
	XRT_DEVICE_INDEX_CONTROLLER,

	XRT_DEVICE_HP_REVERB_G2_CONTROLLER,
	XRT_DEVICE_SAMSUNG_ODYSSEY_CONTROLLER,
	XRT_DEVICE_ML2_CONTROLLER,
	XRT_DEVICE_OPPO_MR_CONTROLLER,

	XRT_DEVICE_HAND_INTERACTION,

	XRT_DEVICE_EYE_GAZE_INTERACTION,

	XRT_DEVICE_PSMV,
	XRT_DEVICE_PSSENSE,
	XRT_DEVICE_HYDRA,

	// Other misc stuff.
	XRT_DEVICE_HAND_TRACKER,
	XRT_DEVICE_REALSENSE,
	XRT_DEVICE_DEPTHAI,

	//! XR_EXT_hand_interaction
	XRT_DEVICE_EXT_HAND_INTERACTION,

	//! XR_HTC_facial_tracking
	XRT_DEVICE_HTC_FACE_TRACKING,

	//! XR_FB_body_tracking
	XRT_DEVICE_FB_BODY_TRACKING,
};

/*!
 * How an xrt_device can be used.
 *
 * @ingroup xrt_iface
 */
enum xrt_device_type
{
	XRT_DEVICE_TYPE_UNKNOWN = 0,
	XRT_DEVICE_TYPE_HMD,
	XRT_DEVICE_TYPE_RIGHT_HAND_CONTROLLER,
	XRT_DEVICE_TYPE_LEFT_HAND_CONTROLLER,
	XRT_DEVICE_TYPE_ANY_HAND_CONTROLLER,
	XRT_DEVICE_TYPE_GENERIC_TRACKER,
	XRT_DEVICE_TYPE_HAND_TRACKER,
	XRT_DEVICE_TYPE_EYE_TRACKER,
	XRT_DEVICE_TYPE_FACE_TRACKER,
	XRT_DEVICE_TYPE_BODY_TRACKER,
};

/*!
 * Base type of this inputs.
 *
 * @ingroup xrt_iface
 */
enum xrt_input_type
{
	// clang-format off
	//! Float input in [0, 1]
	XRT_INPUT_TYPE_VEC1_ZERO_TO_ONE      = 0x00,
	//! Float input in [-1, 1]
	XRT_INPUT_TYPE_VEC1_MINUS_ONE_TO_ONE = 0x01,
	//! Vec2 input, components in [-1, 1]
	XRT_INPUT_TYPE_VEC2_MINUS_ONE_TO_ONE = 0x02,
	//! Vec3 input, components in [-1, 1]
	XRT_INPUT_TYPE_VEC3_MINUS_ONE_TO_ONE = 0x03,
	//! Boolean (digital, binary) input
	XRT_INPUT_TYPE_BOOLEAN               = 0x04,
	//! A tracked pose
	XRT_INPUT_TYPE_POSE                  = 0x05,
	//! A tracked hand
	XRT_INPUT_TYPE_HAND_TRACKING         = 0x06,
	//! A tracked face
	XRT_INPUT_TYPE_FACE_TRACKING         = 0x07,
	//! A tracked body
	XRT_INPUT_TYPE_BODY_TRACKING         = 0x08,
	// clang-format on
};

/*!
 * The number of bits reserved for the input type in @ref xrt_input_name
 *
 * @see xrt_input_name
 * @ingroup xrt_iface
 */
#define XRT_INPUT_TYPE_BITWIDTH 8u

/*!
 * The mask associated with @ref XRT_INPUT_TYPE_BITWIDTH
 *
 * @see xrt_input_name
 * @ingroup xrt_iface
 */

#define XRT_INPUT_TYPE_BITMASK 0xffu

/*!
 * @brief Create an enum value for xrt_input_name that packs an ID and input
 * type.
 *
 * @param id an integer
 * @param type The suffix of an xrt_input_type value name: `XRT_INPUT_TYPE_` is
 * prepended automatically.
 *
 * @see xrt_input_name
 * @ingroup xrt_iface
 */
#define XRT_INPUT_NAME(id, type) ((UINT32_C(id) << XRT_INPUT_TYPE_BITWIDTH) | (uint32_t)XRT_INPUT_TYPE_##type)

/*!
 * @brief Extract the xrt_input_type from an xrt_input_name.
 *
 * @param name A xrt_input_name value
 *
 * @relates xrt_input_name
 * @returns @ref xrt_input_type
 * @ingroup xrt_iface
 */
#define XRT_GET_INPUT_TYPE(name) ((enum xrt_input_type)(name & XRT_INPUT_TYPE_BITMASK))

/*!
 * @brief Extract the xrt_input_name id from an xrt_input_name.
 *
 * @param name A xrt_input_name value
 *
 * @relates xrt_input_name
 * @returns @ref xrt_input_type
 * @ingroup xrt_iface
 */
#define XRT_GET_INPUT_ID(name) ((uint32_t)(name >> XRT_INPUT_TYPE_BITWIDTH))

// clang-format off
#define XRT_INPUT_LIST(_) \
	/** Standard pose used for rendering */  \
	_(XRT_INPUT_GENERIC_HEAD_POSE                      , XRT_INPUT_NAME(0x0000, POSE)) \
	_(XRT_INPUT_GENERIC_HEAD_DETECT                    , XRT_INPUT_NAME(0x0001, BOOLEAN)) \
	_(XRT_INPUT_GENERIC_HAND_TRACKING_LEFT             , XRT_INPUT_NAME(0x0002, HAND_TRACKING)) \
	_(XRT_INPUT_GENERIC_HAND_TRACKING_RIGHT            , XRT_INPUT_NAME(0x0004, HAND_TRACKING)) \
	_(XRT_INPUT_GENERIC_TRACKER_POSE                   , XRT_INPUT_NAME(0x0005, POSE)) \
	/** XR_EXT_palm_pose */ \
	_(XRT_INPUT_GENERIC_PALM_POSE                      , XRT_INPUT_NAME(0x0006, POSE)) \
\
	/** XR_EXT_eye_gaze_interaction */ \
	_(XRT_INPUT_GENERIC_EYE_GAZE_POSE                  , XRT_INPUT_NAME(0x0007, POSE)) \
	/** Standard non-view reference spaces */ \
	_(XRT_INPUT_GENERIC_LOCAL_SPACE_POSE               , XRT_INPUT_NAME(0x0008, POSE)) \
	_(XRT_INPUT_GENERIC_LOCAL_FLOOR_SPACE_POSE         , XRT_INPUT_NAME(0x0009, POSE)) \
	_(XRT_INPUT_GENERIC_STAGE_SPACE_POSE               , XRT_INPUT_NAME(0x000A, POSE)) \
	_(XRT_INPUT_GENERIC_UNBOUNDED_SPACE_POSE           , XRT_INPUT_NAME(0x000B, POSE)) \
\
	_(XRT_INPUT_SIMPLE_SELECT_CLICK                    , XRT_INPUT_NAME(0x0010, BOOLEAN)) \
	_(XRT_INPUT_SIMPLE_MENU_CLICK                      , XRT_INPUT_NAME(0x0011, BOOLEAN)) \
	_(XRT_INPUT_SIMPLE_GRIP_POSE                       , XRT_INPUT_NAME(0x0012, POSE)) \
	_(XRT_INPUT_SIMPLE_AIM_POSE                        , XRT_INPUT_NAME(0x0013, POSE)) \
\
	_(XRT_INPUT_PSMV_PS_CLICK                          , XRT_INPUT_NAME(0x0020, BOOLEAN)) \
	_(XRT_INPUT_PSMV_MOVE_CLICK                        , XRT_INPUT_NAME(0x0021, BOOLEAN)) \
	_(XRT_INPUT_PSMV_START_CLICK                       , XRT_INPUT_NAME(0x0022, BOOLEAN)) \
	_(XRT_INPUT_PSMV_SELECT_CLICK                      , XRT_INPUT_NAME(0x0023, BOOLEAN)) \
	_(XRT_INPUT_PSMV_SQUARE_CLICK                      , XRT_INPUT_NAME(0x0024, BOOLEAN)) \
	_(XRT_INPUT_PSMV_CROSS_CLICK                       , XRT_INPUT_NAME(0x0025, BOOLEAN)) \
	_(XRT_INPUT_PSMV_CIRCLE_CLICK                      , XRT_INPUT_NAME(0x0026, BOOLEAN)) \
	_(XRT_INPUT_PSMV_TRIANGLE_CLICK                    , XRT_INPUT_NAME(0x0027, BOOLEAN)) \
	_(XRT_INPUT_PSMV_TRIGGER_VALUE                     , XRT_INPUT_NAME(0x0028, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_PSMV_GRIP_POSE                         , XRT_INPUT_NAME(0x0029, POSE)) \
	_(XRT_INPUT_PSMV_AIM_POSE                          , XRT_INPUT_NAME(0x002A, POSE)) \
	_(XRT_INPUT_PSMV_BODY_CENTER_POSE                  , XRT_INPUT_NAME(0x002B, POSE)) \
	_(XRT_INPUT_PSMV_BALL_CENTER_POSE                  , XRT_INPUT_NAME(0x002C, POSE)) \
\
	_(XRT_INPUT_HYDRA_1_CLICK                          , XRT_INPUT_NAME(0x0030, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_2_CLICK                          , XRT_INPUT_NAME(0x0031, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_3_CLICK                          , XRT_INPUT_NAME(0x0032, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_4_CLICK                          , XRT_INPUT_NAME(0x0033, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_MIDDLE_CLICK                     , XRT_INPUT_NAME(0x0034, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_BUMPER_CLICK                     , XRT_INPUT_NAME(0x0035, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_JOYSTICK_CLICK                   , XRT_INPUT_NAME(0x0036, BOOLEAN)) \
	_(XRT_INPUT_HYDRA_JOYSTICK_VALUE                   , XRT_INPUT_NAME(0x0037, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_HYDRA_TRIGGER_VALUE                    , XRT_INPUT_NAME(0x0038, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_HYDRA_POSE                             , XRT_INPUT_NAME(0x0039, POSE)) \
\
	_(XRT_INPUT_DAYDREAM_TOUCHPAD_CLICK                , XRT_INPUT_NAME(0x0040, BOOLEAN)) \
	_(XRT_INPUT_DAYDREAM_BAR_CLICK                     , XRT_INPUT_NAME(0x0041, BOOLEAN)) \
	_(XRT_INPUT_DAYDREAM_CIRCLE_CLICK                  , XRT_INPUT_NAME(0x0042, BOOLEAN)) \
	_(XRT_INPUT_DAYDREAM_VOLUP_CLICK                   , XRT_INPUT_NAME(0x0043, BOOLEAN)) \
	_(XRT_INPUT_DAYDREAM_VOLDN_CLICK                   , XRT_INPUT_NAME(0x0044, BOOLEAN)) \
	_(XRT_INPUT_DAYDREAM_TOUCHPAD                      , XRT_INPUT_NAME(0x0045, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_DAYDREAM_POSE                          , XRT_INPUT_NAME(0x0046, POSE)) \
	_(XRT_INPUT_DAYDREAM_TOUCHPAD_TOUCH                , XRT_INPUT_NAME(0x0047, BOOLEAN)) \
\
	_(XRT_INPUT_INDEX_SYSTEM_CLICK                     , XRT_INPUT_NAME(0x0050, BOOLEAN)) \
	_(XRT_INPUT_INDEX_SYSTEM_TOUCH                     , XRT_INPUT_NAME(0x0051, BOOLEAN)) \
	_(XRT_INPUT_INDEX_A_CLICK                          , XRT_INPUT_NAME(0x0052, BOOLEAN)) \
	_(XRT_INPUT_INDEX_A_TOUCH                          , XRT_INPUT_NAME(0x0053, BOOLEAN)) \
	_(XRT_INPUT_INDEX_B_CLICK                          , XRT_INPUT_NAME(0x0054, BOOLEAN)) \
	_(XRT_INPUT_INDEX_B_TOUCH                          , XRT_INPUT_NAME(0x0055, BOOLEAN)) \
	_(XRT_INPUT_INDEX_SQUEEZE_VALUE                    , XRT_INPUT_NAME(0x0056, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_INDEX_SQUEEZE_FORCE                    , XRT_INPUT_NAME(0x0057, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_INDEX_TRIGGER_CLICK                    , XRT_INPUT_NAME(0x0058, BOOLEAN)) \
	_(XRT_INPUT_INDEX_TRIGGER_VALUE                    , XRT_INPUT_NAME(0x0059, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_INDEX_TRIGGER_TOUCH                    , XRT_INPUT_NAME(0x005A, BOOLEAN)) \
	_(XRT_INPUT_INDEX_THUMBSTICK                       , XRT_INPUT_NAME(0x005B, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_INDEX_THUMBSTICK_CLICK                 , XRT_INPUT_NAME(0x005D, BOOLEAN)) \
	_(XRT_INPUT_INDEX_THUMBSTICK_TOUCH                 , XRT_INPUT_NAME(0x005E, BOOLEAN)) \
	_(XRT_INPUT_INDEX_TRACKPAD                         , XRT_INPUT_NAME(0x005F, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_INDEX_TRACKPAD_FORCE                   , XRT_INPUT_NAME(0x0061, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_INDEX_TRACKPAD_TOUCH                   , XRT_INPUT_NAME(0x0062, BOOLEAN)) \
	_(XRT_INPUT_INDEX_GRIP_POSE                        , XRT_INPUT_NAME(0x0063, POSE)) \
	_(XRT_INPUT_INDEX_AIM_POSE                         , XRT_INPUT_NAME(0x0064, POSE)) \
\
	_(XRT_INPUT_VIVE_SYSTEM_CLICK                      , XRT_INPUT_NAME(0x0070, BOOLEAN)) \
	_(XRT_INPUT_VIVE_SQUEEZE_CLICK                     , XRT_INPUT_NAME(0x0071, BOOLEAN)) \
	_(XRT_INPUT_VIVE_MENU_CLICK                        , XRT_INPUT_NAME(0x0072, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRIGGER_CLICK                     , XRT_INPUT_NAME(0x0073, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRIGGER_VALUE                     , XRT_INPUT_NAME(0x0074, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_VIVE_TRACKPAD                          , XRT_INPUT_NAME(0x0075, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_VIVE_TRACKPAD_CLICK                    , XRT_INPUT_NAME(0x0076, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKPAD_TOUCH                    , XRT_INPUT_NAME(0x0077, BOOLEAN)) \
	_(XRT_INPUT_VIVE_GRIP_POSE                         , XRT_INPUT_NAME(0x0078, POSE)) \
	_(XRT_INPUT_VIVE_AIM_POSE                          , XRT_INPUT_NAME(0x0079, POSE)) \
\
	_(XRT_INPUT_VIVEPRO_SYSTEM_CLICK                   , XRT_INPUT_NAME(0x0080, BOOLEAN)) \
	_(XRT_INPUT_VIVEPRO_VOLUP_CLICK                    , XRT_INPUT_NAME(0x0081, BOOLEAN)) \
	_(XRT_INPUT_VIVEPRO_VOLDN_CLICK                    , XRT_INPUT_NAME(0x0082, BOOLEAN)) \
	_(XRT_INPUT_VIVEPRO_MUTE_MIC_CLICK                 , XRT_INPUT_NAME(0x0083, BOOLEAN)) \
\
	_(XRT_INPUT_WMR_MENU_CLICK                         , XRT_INPUT_NAME(0x0090, BOOLEAN)) \
	_(XRT_INPUT_WMR_SQUEEZE_CLICK                      , XRT_INPUT_NAME(0x0091, BOOLEAN)) \
	_(XRT_INPUT_WMR_TRIGGER_VALUE                      , XRT_INPUT_NAME(0x0092, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_WMR_THUMBSTICK_CLICK                   , XRT_INPUT_NAME(0x0093, BOOLEAN)) \
	_(XRT_INPUT_WMR_THUMBSTICK                         , XRT_INPUT_NAME(0x0094, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_WMR_TRACKPAD_CLICK                     , XRT_INPUT_NAME(0x0095, BOOLEAN)) \
	_(XRT_INPUT_WMR_TRACKPAD_TOUCH                     , XRT_INPUT_NAME(0x0096, BOOLEAN)) \
	_(XRT_INPUT_WMR_TRACKPAD                           , XRT_INPUT_NAME(0x0097, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_WMR_GRIP_POSE                          , XRT_INPUT_NAME(0x0098, POSE)) \
	_(XRT_INPUT_WMR_AIM_POSE                           , XRT_INPUT_NAME(0x0099, POSE)) \
	_(XRT_INPUT_WMR_HOME_CLICK                         , XRT_INPUT_NAME(0x009A, BOOLEAN)) \
\
	_(XRT_INPUT_XBOX_MENU_CLICK                        , XRT_INPUT_NAME(0x00A0, BOOLEAN)) \
	_(XRT_INPUT_XBOX_VIEW_CLICK                        , XRT_INPUT_NAME(0x00A1, BOOLEAN)) \
	_(XRT_INPUT_XBOX_A_CLICK                           , XRT_INPUT_NAME(0x00A2, BOOLEAN)) \
	_(XRT_INPUT_XBOX_B_CLICK                           , XRT_INPUT_NAME(0x00A3, BOOLEAN)) \
	_(XRT_INPUT_XBOX_X_CLICK                           , XRT_INPUT_NAME(0x00A4, BOOLEAN)) \
	_(XRT_INPUT_XBOX_Y_CLICK                           , XRT_INPUT_NAME(0x00A5, BOOLEAN)) \
	_(XRT_INPUT_XBOX_DPAD_DOWN_CLICK                   , XRT_INPUT_NAME(0x00A6, BOOLEAN)) \
	_(XRT_INPUT_XBOX_DPAD_RIGHT_CLICK                  , XRT_INPUT_NAME(0x00A7, BOOLEAN)) \
	_(XRT_INPUT_XBOX_DPAD_UP_CLICK                     , XRT_INPUT_NAME(0x00A8, BOOLEAN)) \
	_(XRT_INPUT_XBOX_DPAD_LEFT_CLICK                   , XRT_INPUT_NAME(0x00A9, BOOLEAN)) \
	_(XRT_INPUT_XBOX_SHOULDER_LEFT_CLICK               , XRT_INPUT_NAME(0x00AA, BOOLEAN)) \
	_(XRT_INPUT_XBOX_SHOULDER_RIGHT_CLICK              , XRT_INPUT_NAME(0x00AB, BOOLEAN)) \
	_(XRT_INPUT_XBOX_THUMBSTICK_LEFT_CLICK             , XRT_INPUT_NAME(0x00AC, BOOLEAN)) \
	_(XRT_INPUT_XBOX_THUMBSTICK_LEFT                   , XRT_INPUT_NAME(0x00AD, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_XBOX_THUMBSTICK_RIGHT_CLICK            , XRT_INPUT_NAME(0x00AE, BOOLEAN)) \
	_(XRT_INPUT_XBOX_THUMBSTICK_RIGHT                  , XRT_INPUT_NAME(0x00AF, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_XBOX_LEFT_TRIGGER_VALUE                , XRT_INPUT_NAME(0x00B0, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_XBOX_RIGHT_TRIGGER_VALUE               , XRT_INPUT_NAME(0x00B1, VEC1_ZERO_TO_ONE)) \
\
	_(XRT_INPUT_GO_SYSTEM_CLICK                        , XRT_INPUT_NAME(0x00B0, BOOLEAN)) \
	_(XRT_INPUT_GO_TRIGGER_CLICK                       , XRT_INPUT_NAME(0x00B1, BOOLEAN)) \
	_(XRT_INPUT_GO_BACK_CLICK                          , XRT_INPUT_NAME(0x00B2, BOOLEAN)) \
	_(XRT_INPUT_GO_TRACKPAD_CLICK                      , XRT_INPUT_NAME(0x00B3, BOOLEAN)) \
	_(XRT_INPUT_GO_TRACKPAD_TOUCH                      , XRT_INPUT_NAME(0x00B4, BOOLEAN)) \
	_(XRT_INPUT_GO_TRACKPAD                            , XRT_INPUT_NAME(0x00B5, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_GO_GRIP_POSE                           , XRT_INPUT_NAME(0x00B6, POSE)) \
	_(XRT_INPUT_GO_AIM_POSE                            , XRT_INPUT_NAME(0x00B7, POSE)) \
\
	_(XRT_INPUT_TOUCH_X_CLICK                          , XRT_INPUT_NAME(0x00C0, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_X_TOUCH                          , XRT_INPUT_NAME(0x00C1, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_Y_CLICK                          , XRT_INPUT_NAME(0x00C2, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_Y_TOUCH                          , XRT_INPUT_NAME(0x00C3, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_MENU_CLICK                       , XRT_INPUT_NAME(0x00C4, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_A_CLICK                          , XRT_INPUT_NAME(0x00C5, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_A_TOUCH                          , XRT_INPUT_NAME(0x00C6, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_B_CLICK                          , XRT_INPUT_NAME(0x00C7, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_B_TOUCH                          , XRT_INPUT_NAME(0x00C8, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_SYSTEM_CLICK                     , XRT_INPUT_NAME(0x00C9, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_SQUEEZE_VALUE                    , XRT_INPUT_NAME(0x00CA, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_TOUCH_TRIGGER_TOUCH                    , XRT_INPUT_NAME(0x00CB, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_TRIGGER_VALUE                    , XRT_INPUT_NAME(0x00CC, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_TOUCH_THUMBSTICK_CLICK                 , XRT_INPUT_NAME(0x00CD, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_THUMBSTICK_TOUCH                 , XRT_INPUT_NAME(0x00CE, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_THUMBSTICK                       , XRT_INPUT_NAME(0x00CF, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_TOUCH_THUMBREST_TOUCH                  , XRT_INPUT_NAME(0x00D0, BOOLEAN)) \
	_(XRT_INPUT_TOUCH_GRIP_POSE                        , XRT_INPUT_NAME(0x00D1, POSE)) \
	_(XRT_INPUT_TOUCH_AIM_POSE                         , XRT_INPUT_NAME(0x00D2, POSE)) \
\
	_(XRT_INPUT_HAND_SELECT_VALUE                      , XRT_INPUT_NAME(0x00E0, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_HAND_SQUEEZE_VALUE                     , XRT_INPUT_NAME(0x00E1, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_HAND_GRIP_POSE                         , XRT_INPUT_NAME(0x00E2, POSE)) \
	_(XRT_INPUT_HAND_AIM_POSE                          , XRT_INPUT_NAME(0x00E3, POSE)) \
\
	_(XRT_INPUT_G2_CONTROLLER_X_CLICK                  , XRT_INPUT_NAME(0x00F0, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_Y_CLICK                  , XRT_INPUT_NAME(0x00F1, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_A_CLICK                  , XRT_INPUT_NAME(0x00F2, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_B_CLICK                  , XRT_INPUT_NAME(0x00F3, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_MENU_CLICK               , XRT_INPUT_NAME(0x00F4, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_SQUEEZE_VALUE            , XRT_INPUT_NAME(0x00F5, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_G2_CONTROLLER_TRIGGER_VALUE            , XRT_INPUT_NAME(0x00F6, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_G2_CONTROLLER_THUMBSTICK_CLICK         , XRT_INPUT_NAME(0x00F7, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_THUMBSTICK               , XRT_INPUT_NAME(0x00F8, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_G2_CONTROLLER_GRIP_POSE                , XRT_INPUT_NAME(0x00F9, POSE)) \
	_(XRT_INPUT_G2_CONTROLLER_AIM_POSE                 , XRT_INPUT_NAME(0x00FA, POSE)) \
	_(XRT_INPUT_G2_CONTROLLER_HOME_CLICK               , XRT_INPUT_NAME(0x00FB, BOOLEAN)) \
	_(XRT_INPUT_G2_CONTROLLER_SQUEEZE_CLICK            , XRT_INPUT_NAME(0x00FC, BOOLEAN)) \
\
	_(XRT_INPUT_ODYSSEY_CONTROLLER_MENU_CLICK          , XRT_INPUT_NAME(0x0100, BOOLEAN)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_SQUEEZE_CLICK       , XRT_INPUT_NAME(0x0101, BOOLEAN)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_TRIGGER_VALUE       , XRT_INPUT_NAME(0x0102, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_THUMBSTICK_CLICK    , XRT_INPUT_NAME(0x0103, BOOLEAN)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_THUMBSTICK          , XRT_INPUT_NAME(0x0104, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_TRACKPAD_CLICK      , XRT_INPUT_NAME(0x0105, BOOLEAN)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_TRACKPAD_TOUCH      , XRT_INPUT_NAME(0x0106, BOOLEAN)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_TRACKPAD            , XRT_INPUT_NAME(0x0107, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_GRIP_POSE           , XRT_INPUT_NAME(0x0108, POSE)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_AIM_POSE            , XRT_INPUT_NAME(0x0109, POSE)) \
	_(XRT_INPUT_ODYSSEY_CONTROLLER_HOME_CLICK          , XRT_INPUT_NAME(0x010A, BOOLEAN)) \
\
	_(XRT_INPUT_ML2_CONTROLLER_MENU_CLICK              , XRT_INPUT_NAME(0x0200, BOOLEAN)) \
	_(XRT_INPUT_ML2_CONTROLLER_SELECT_CLICK            , XRT_INPUT_NAME(0x0201, BOOLEAN)) \
	_(XRT_INPUT_ML2_CONTROLLER_TRIGGER_CLICK           , XRT_INPUT_NAME(0x0202, BOOLEAN)) \
	_(XRT_INPUT_ML2_CONTROLLER_TRIGGER_VALUE           , XRT_INPUT_NAME(0x0203, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_ML2_CONTROLLER_TRACKPAD_CLICK          , XRT_INPUT_NAME(0x0204, BOOLEAN)) \
	_(XRT_INPUT_ML2_CONTROLLER_TRACKPAD_TOUCH          , XRT_INPUT_NAME(0x0205, BOOLEAN)) \
	_(XRT_INPUT_ML2_CONTROLLER_TRACKPAD_FORCE          , XRT_INPUT_NAME(0x0206, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_ML2_CONTROLLER_TRACKPAD                , XRT_INPUT_NAME(0x0207, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_ML2_CONTROLLER_GRIP_POSE               , XRT_INPUT_NAME(0x0208, POSE)) \
	_(XRT_INPUT_ML2_CONTROLLER_AIM_POSE                , XRT_INPUT_NAME(0x0209, POSE)) \
	_(XRT_INPUT_ML2_CONTROLLER_SHOULDER_CLICK          , XRT_INPUT_NAME(0x020A, BOOLEAN)) \
\
	_(XRT_INPUT_VIVE_TRACKER_SYSTEM_CLICK              , XRT_INPUT_NAME(0x0210, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKER_MENU_CLICK                , XRT_INPUT_NAME(0x0211, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKER_TRIGGER_CLICK             , XRT_INPUT_NAME(0x0212, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKER_SQUEEZE_CLICK             , XRT_INPUT_NAME(0x0213, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKER_TRIGGER_VALUE             , XRT_INPUT_NAME(0x0214, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_VIVE_TRACKER_TRACKPAD                  , XRT_INPUT_NAME(0x0215, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_VIVE_TRACKER_TRACKPAD_CLICK            , XRT_INPUT_NAME(0x0216, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKER_TRACKPAD_TOUCH            , XRT_INPUT_NAME(0x0217, BOOLEAN)) \
	_(XRT_INPUT_VIVE_TRACKER_GRIP_POSE                 , XRT_INPUT_NAME(0x0218, POSE)) \
\
	_(XRT_INPUT_PSSENSE_PS_CLICK                       , XRT_INPUT_NAME(0x0300, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_SHARE_CLICK                    , XRT_INPUT_NAME(0x0301, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_OPTIONS_CLICK                  , XRT_INPUT_NAME(0x0302, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_SQUARE_CLICK                   , XRT_INPUT_NAME(0x0303, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_SQUARE_TOUCH                   , XRT_INPUT_NAME(0x0304, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_TRIANGLE_CLICK                 , XRT_INPUT_NAME(0x0305, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_TRIANGLE_TOUCH                 , XRT_INPUT_NAME(0x0306, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_CROSS_CLICK                    , XRT_INPUT_NAME(0x0307, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_CROSS_TOUCH                    , XRT_INPUT_NAME(0x0308, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_CIRCLE_CLICK                   , XRT_INPUT_NAME(0x0309, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_CIRCLE_TOUCH                   , XRT_INPUT_NAME(0x030a, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_SQUEEZE_CLICK                  , XRT_INPUT_NAME(0x030b, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_SQUEEZE_TOUCH                  , XRT_INPUT_NAME(0x030c, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_SQUEEZE_PROXIMITY              , XRT_INPUT_NAME(0x030d, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_PSSENSE_TRIGGER_CLICK                  , XRT_INPUT_NAME(0x030e, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_TRIGGER_TOUCH                  , XRT_INPUT_NAME(0x030f, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_TRIGGER_VALUE                  , XRT_INPUT_NAME(0x0310, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_PSSENSE_TRIGGER_PROXIMITY              , XRT_INPUT_NAME(0x0311, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_PSSENSE_THUMBSTICK                     , XRT_INPUT_NAME(0x0312, VEC2_MINUS_ONE_TO_ONE)) \
	_(XRT_INPUT_PSSENSE_THUMBSTICK_CLICK               , XRT_INPUT_NAME(0x0313, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_THUMBSTICK_TOUCH               , XRT_INPUT_NAME(0x0314, BOOLEAN)) \
	_(XRT_INPUT_PSSENSE_GRIP_POSE                      , XRT_INPUT_NAME(0x0315, POSE)) \
	_(XRT_INPUT_PSSENSE_AIM_POSE                       , XRT_INPUT_NAME(0x0316, POSE)) \
\
	/** XR_EXT_hand_interaction */ \
	_(XRT_INPUT_HAND_PINCH_POSE                        , XRT_INPUT_NAME(0x0401, POSE)) \
	_(XRT_INPUT_HAND_POKE_POSE                         , XRT_INPUT_NAME(0x0402, POSE)) \
	_(XRT_INPUT_HAND_PINCH_VALUE                       , XRT_INPUT_NAME(0x0403, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_HAND_AIM_ACTIVATE_VALUE                , XRT_INPUT_NAME(0x0404, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_HAND_GRASP_VALUE                       , XRT_INPUT_NAME(0x0405, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_HAND_PINCH_READY                       , XRT_INPUT_NAME(0x0406, BOOLEAN)) \
	_(XRT_INPUT_HAND_AIM_ACTIVATE_READY                , XRT_INPUT_NAME(0x0407, BOOLEAN)) \
	_(XRT_INPUT_HAND_GRASP_READY                       , XRT_INPUT_NAME(0x0408, BOOLEAN)) \
\
	_(XRT_INPUT_OPPO_MR_X_CLICK                        , XRT_INPUT_NAME(0x0500, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_X_TOUCH                        , XRT_INPUT_NAME(0x0501, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_Y_CLICK                        , XRT_INPUT_NAME(0x0502, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_Y_TOUCH                        , XRT_INPUT_NAME(0x0503, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_MENU_CLICK                     , XRT_INPUT_NAME(0x0504, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_HEART_RATE_VALUE               , XRT_INPUT_NAME(0x0505, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_OPPO_MR_A_CLICK                        , XRT_INPUT_NAME(0x0506, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_A_TOUCH                        , XRT_INPUT_NAME(0x0507, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_B_CLICK                        , XRT_INPUT_NAME(0x0508, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_B_TOUCH                        , XRT_INPUT_NAME(0x0509, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_HOME_CLICK                     , XRT_INPUT_NAME(0x050A, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_SQUEEZE_VALUE                  , XRT_INPUT_NAME(0x050B, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_OPPO_MR_TRIGGER_TOUCH                  , XRT_INPUT_NAME(0x050C, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_TRIGGER_VALUE                  , XRT_INPUT_NAME(0x050D, VEC1_ZERO_TO_ONE)) \
	_(XRT_INPUT_OPPO_MR_GRIP_POSE                      , XRT_INPUT_NAME(0x050E, POSE)) \
	_(XRT_INPUT_OPPO_MR_AIM_POSE                       , XRT_INPUT_NAME(0x050F, POSE)) \
	_(XRT_INPUT_OPPO_MR_THUMBSTICK_CLICK               , XRT_INPUT_NAME(0x0510, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_THUMBSTICK_TOUCH               , XRT_INPUT_NAME(0x0511, BOOLEAN)) \
	_(XRT_INPUT_OPPO_MR_THUMBSTICK                     , XRT_INPUT_NAME(0x0512, VEC2_MINUS_ONE_TO_ONE)) \
\
	_(XRT_INPUT_GENERIC_FACE_TRACKING                  , XRT_INPUT_NAME(0x0600, FACE_TRACKING)) \
\
	_(XRT_INPUT_HTC_EYE_FACE_TRACKING                  , XRT_INPUT_NAME(0x0601, FACE_TRACKING)) \
	_(XRT_INPUT_HTC_LIP_FACE_TRACKING                  , XRT_INPUT_NAME(0x0602, FACE_TRACKING)) \
\
	_(XRT_INPUT_GENERIC_BODY_TRACKING                  , XRT_INPUT_NAME(0x0700, BODY_TRACKING)) \
	_(XRT_INPUT_FB_BODY_TRACKING                       , XRT_INPUT_NAME(0x0701, BODY_TRACKING)) \
	_(XRT_INPUT_META_FULL_BODY_TRACKING                , XRT_INPUT_NAME(0x0702, BODY_TRACKING))

// clang-format on


/*!
 * Every internal input source known to monado with a baked in type.
 * Values are maintained in XRT_INPUT_LIST.
 *
 * @see xrt_input_type
 * @ingroup xrt_iface
 */
enum xrt_input_name
{
#define XRT_INPUT_LIST_TO_NAME_VALUE(NAME, VALUE) NAME = VALUE,

	XRT_INPUT_LIST(XRT_INPUT_LIST_TO_NAME_VALUE)

#undef XRT_INPUT_LIST_TO_NAME_VALUE
};

/*!
 * Number of joints in a hand. Corresponds to XR_HAND_JOINT_COUNT_EXT.
 *
 * @see xrt_hand_joint
 * @ingroup xrt_iface
 */
#define XRT_HAND_JOINT_COUNT 26

/*!
 * Number of joints in a hand. Corresponds to XrHandJointEXT.
 *
 * @ingroup xrt_iface
 */
enum xrt_hand_joint
{
	XRT_HAND_JOINT_PALM = 0,
	XRT_HAND_JOINT_WRIST = 1,
	XRT_HAND_JOINT_THUMB_METACARPAL = 2,
	XRT_HAND_JOINT_THUMB_PROXIMAL = 3,
	XRT_HAND_JOINT_THUMB_DISTAL = 4,
	XRT_HAND_JOINT_THUMB_TIP = 5,
	XRT_HAND_JOINT_INDEX_METACARPAL = 6,
	XRT_HAND_JOINT_INDEX_PROXIMAL = 7,
	XRT_HAND_JOINT_INDEX_INTERMEDIATE = 8,
	XRT_HAND_JOINT_INDEX_DISTAL = 9,
	XRT_HAND_JOINT_INDEX_TIP = 10,
	XRT_HAND_JOINT_MIDDLE_METACARPAL = 11,
	XRT_HAND_JOINT_MIDDLE_PROXIMAL = 12,
	XRT_HAND_JOINT_MIDDLE_INTERMEDIATE = 13,
	XRT_HAND_JOINT_MIDDLE_DISTAL = 14,
	XRT_HAND_JOINT_MIDDLE_TIP = 15,
	XRT_HAND_JOINT_RING_METACARPAL = 16,
	XRT_HAND_JOINT_RING_PROXIMAL = 17,
	XRT_HAND_JOINT_RING_INTERMEDIATE = 18,
	XRT_HAND_JOINT_RING_DISTAL = 19,
	XRT_HAND_JOINT_RING_TIP = 20,
	XRT_HAND_JOINT_LITTLE_METACARPAL = 21,
	XRT_HAND_JOINT_LITTLE_PROXIMAL = 22,
	XRT_HAND_JOINT_LITTLE_INTERMEDIATE = 23,
	XRT_HAND_JOINT_LITTLE_DISTAL = 24,
	XRT_HAND_JOINT_LITTLE_TIP = 25,
	XRT_HAND_JOINT_MAX_ENUM = 0x7FFFFFFF
};

/*!
 * Enumeration for left and right hand.
 *
 * @ingroup xrt_iface
 */
enum xrt_hand
{
	XRT_HAND_LEFT = 0,
	XRT_HAND_RIGHT = 1,
};

/*!
 * Location of a single hand joint. Corresponds to XrHandJointLocationEXT.
 *
 * @ingroup xrt_iface
 */
struct xrt_hand_joint_value
{
	struct xrt_space_relation relation;
	float radius;
};

/*!
 * Number of fingers on a hand.
 *
 * @ingroup xrt_iface
 */
#define XRT_FINGER_COUNT 5

/*!
 * Names for fingers on a hand.
 *
 * @ingroup xrt_iface
 */
enum xrt_finger
{
	XRT_FINGER_LITTLE = 0,
	XRT_FINGER_RING,
	XRT_FINGER_MIDDLE,
	XRT_FINGER_INDEX,
	XRT_FINGER_THUMB
};

/*!
 * Joint set type used for hand tracking. Corresponds to XrHandJointSetEXT.
 *
 * @ingroup xrt_iface
 */
struct xrt_hand_joint_set
{
	union {
		struct xrt_hand_joint_value hand_joint_set_default[XRT_HAND_JOINT_COUNT];
	} values;

	// in driver global space, without tracking_origin offset
	struct xrt_space_relation hand_pose;
	bool is_active;
};

/*!
 * A union of all input types.
 *
 * @see xrt_input_type
 * @ingroup xrt_iface math
 */
union xrt_input_value {
	struct xrt_vec1 vec1;
	struct xrt_vec2 vec2;
	bool boolean;
};

/*!
 * The number of bits reserved for the input type in @ref xrt_output_name
 * @see xrt_output_type
 * @ingroup xrt_iface
 */
#define XRT_OUTPUT_TYPE_BITWIDTH 8u

/*!
 * The mask associated with @ref XRT_OUTPUT_TYPE_BITWIDTH
 * @see xrt_output_type
 * @ingroup xrt_iface
 */
#define XRT_OUTPUT_TYPE_BITMASK 0xffu

/*!
 * Base type of this output.
 *
 * @ingroup xrt_iface
 */
enum xrt_output_type
{
	// clang-format off
	XRT_OUTPUT_TYPE_VIBRATION             = 0x00,
	XRT_OUTPUT_TYPE_FORCE_FEEDBACK        = 0x01,
	// clang-format on
};

#define XRT_OUTPUT_NAME(id, type) ((UINT32_C(id) << XRT_OUTPUT_TYPE_BITWIDTH) | (uint32_t)XRT_OUTPUT_TYPE_##type)

enum xrt_eye_expression_htc
{
	XRT_EYE_EXPRESSION_LEFT_BLINK_HTC = 0,
	XRT_EYE_EXPRESSION_LEFT_WIDE_HTC = 1,
	XRT_EYE_EXPRESSION_RIGHT_BLINK_HTC = 2,
	XRT_EYE_EXPRESSION_RIGHT_WIDE_HTC = 3,
	XRT_EYE_EXPRESSION_LEFT_SQUEEZE_HTC = 4,
	XRT_EYE_EXPRESSION_RIGHT_SQUEEZE_HTC = 5,
	XRT_EYE_EXPRESSION_LEFT_DOWN_HTC = 6,
	XRT_EYE_EXPRESSION_RIGHT_DOWN_HTC = 7,
	XRT_EYE_EXPRESSION_LEFT_OUT_HTC = 8,
	XRT_EYE_EXPRESSION_RIGHT_IN_HTC = 9,
	XRT_EYE_EXPRESSION_LEFT_IN_HTC = 10,
	XRT_EYE_EXPRESSION_RIGHT_OUT_HTC = 11,
	XRT_EYE_EXPRESSION_LEFT_UP_HTC = 12,
	XRT_EYE_EXPRESSION_RIGHT_UP_HTC = 13
};

enum xrt_lip_expression_htc
{
	XRT_LIP_EXPRESSION_JAW_RIGHT_HTC = 0,
	XRT_LIP_EXPRESSION_JAW_LEFT_HTC = 1,
	XRT_LIP_EXPRESSION_JAW_FORWARD_HTC = 2,
	XRT_LIP_EXPRESSION_JAW_OPEN_HTC = 3,
	XRT_LIP_EXPRESSION_MOUTH_APE_SHAPE_HTC = 4,
	XRT_LIP_EXPRESSION_MOUTH_UPPER_RIGHT_HTC = 5,
	XRT_LIP_EXPRESSION_MOUTH_UPPER_LEFT_HTC = 6,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_RIGHT_HTC = 7,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_LEFT_HTC = 8,
	XRT_LIP_EXPRESSION_MOUTH_UPPER_OVERTURN_HTC = 9,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_OVERTURN_HTC = 10,
	XRT_LIP_EXPRESSION_MOUTH_POUT_HTC = 11,
	XRT_LIP_EXPRESSION_MOUTH_SMILE_RIGHT_HTC = 12,
	XRT_LIP_EXPRESSION_MOUTH_SMILE_LEFT_HTC = 13,
	XRT_LIP_EXPRESSION_MOUTH_SAD_RIGHT_HTC = 14,
	XRT_LIP_EXPRESSION_MOUTH_SAD_LEFT_HTC = 15,
	XRT_LIP_EXPRESSION_CHEEK_PUFF_RIGHT_HTC = 16,
	XRT_LIP_EXPRESSION_CHEEK_PUFF_LEFT_HTC = 17,
	XRT_LIP_EXPRESSION_CHEEK_SUCK_HTC = 18,
	XRT_LIP_EXPRESSION_MOUTH_UPPER_UPRIGHT_HTC = 19,
	XRT_LIP_EXPRESSION_MOUTH_UPPER_UPLEFT_HTC = 20,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_DOWNRIGHT_HTC = 21,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_DOWNLEFT_HTC = 22,
	XRT_LIP_EXPRESSION_MOUTH_UPPER_INSIDE_HTC = 23,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_INSIDE_HTC = 24,
	XRT_LIP_EXPRESSION_MOUTH_LOWER_OVERLAY_HTC = 25,
	XRT_LIP_EXPRESSION_TONGUE_LONGSTEP1_HTC = 26,
	XRT_LIP_EXPRESSION_TONGUE_LEFT_HTC = 27,
	XRT_LIP_EXPRESSION_TONGUE_RIGHT_HTC = 28,
	XRT_LIP_EXPRESSION_TONGUE_UP_HTC = 29,
	XRT_LIP_EXPRESSION_TONGUE_DOWN_HTC = 30,
	XRT_LIP_EXPRESSION_TONGUE_ROLL_HTC = 31,
	XRT_LIP_EXPRESSION_TONGUE_LONGSTEP2_HTC = 32,
	XRT_LIP_EXPRESSION_TONGUE_UPRIGHT_MORPH_HTC = 33,
	XRT_LIP_EXPRESSION_TONGUE_UPLEFT_MORPH_HTC = 34,
	XRT_LIP_EXPRESSION_TONGUE_DOWNRIGHT_MORPH_HTC = 35,
	XRT_LIP_EXPRESSION_TONGUE_DOWNLEFT_MORPH_HTC = 36
};

enum xrt_facial_tracking_type_htc
{
	XRT_FACIAL_TRACKING_TYPE_EYE_DEFAULT_HTC = 1,
	XRT_FACIAL_TRACKING_TYPE_LIP_DEFAULT_HTC = 2
};

#define XRT_FACIAL_EXPRESSION_EYE_COUNT_HTC 14
#define XRT_FACIAL_EXPRESSION_LIP_COUNT_HTC 37

struct xrt_facial_base_expression_set_htc
{
	uint64_t sample_time_ns;
	bool is_active;
};

struct xrt_facial_eye_expression_set_htc
{
	struct xrt_facial_base_expression_set_htc base;
	// ordered by xrt_eye_expression_htc
	float expression_weights[XRT_FACIAL_EXPRESSION_EYE_COUNT_HTC];
};

struct xrt_facial_lip_expression_set_htc
{
	struct xrt_facial_base_expression_set_htc base;
	// ordered by xrt_lip_expression_htc
	float expression_weights[XRT_FACIAL_EXPRESSION_LIP_COUNT_HTC];
};

struct xrt_facial_expression_set
{
	union {
		struct xrt_facial_base_expression_set_htc base_expression_set_htc;
		struct xrt_facial_eye_expression_set_htc eye_expression_set_htc;
		struct xrt_facial_lip_expression_set_htc lip_expression_set_htc;
	};
};

// XR_FB_body_tracking
enum xrt_body_joint_fb
{
	XRT_BODY_JOINT_ROOT_FB = 0,
	XRT_BODY_JOINT_HIPS_FB = 1,
	XRT_BODY_JOINT_SPINE_LOWER_FB = 2,
	XRT_BODY_JOINT_SPINE_MIDDLE_FB = 3,
	XRT_BODY_JOINT_SPINE_UPPER_FB = 4,
	XRT_BODY_JOINT_CHEST_FB = 5,
	XRT_BODY_JOINT_NECK_FB = 6,
	XRT_BODY_JOINT_HEAD_FB = 7,
	XRT_BODY_JOINT_LEFT_SHOULDER_FB = 8,
	XRT_BODY_JOINT_LEFT_SCAPULA_FB = 9,
	XRT_BODY_JOINT_LEFT_ARM_UPPER_FB = 10,
	XRT_BODY_JOINT_LEFT_ARM_LOWER_FB = 11,
	XRT_BODY_JOINT_LEFT_HAND_WRIST_TWIST_FB = 12,
	XRT_BODY_JOINT_RIGHT_SHOULDER_FB = 13,
	XRT_BODY_JOINT_RIGHT_SCAPULA_FB = 14,
	XRT_BODY_JOINT_RIGHT_ARM_UPPER_FB = 15,
	XRT_BODY_JOINT_RIGHT_ARM_LOWER_FB = 16,
	XRT_BODY_JOINT_RIGHT_HAND_WRIST_TWIST_FB = 17,
	XRT_BODY_JOINT_LEFT_HAND_PALM_FB = 18,
	XRT_BODY_JOINT_LEFT_HAND_WRIST_FB = 19,
	XRT_BODY_JOINT_LEFT_HAND_THUMB_METACARPAL_FB = 20,
	XRT_BODY_JOINT_LEFT_HAND_THUMB_PROXIMAL_FB = 21,
	XRT_BODY_JOINT_LEFT_HAND_THUMB_DISTAL_FB = 22,
	XRT_BODY_JOINT_LEFT_HAND_THUMB_TIP_FB = 23,
	XRT_BODY_JOINT_LEFT_HAND_INDEX_METACARPAL_FB = 24,
	XRT_BODY_JOINT_LEFT_HAND_INDEX_PROXIMAL_FB = 25,
	XRT_BODY_JOINT_LEFT_HAND_INDEX_INTERMEDIATE_FB = 26,
	XRT_BODY_JOINT_LEFT_HAND_INDEX_DISTAL_FB = 27,
	XRT_BODY_JOINT_LEFT_HAND_INDEX_TIP_FB = 28,
	XRT_BODY_JOINT_LEFT_HAND_MIDDLE_METACARPAL_FB = 29,
	XRT_BODY_JOINT_LEFT_HAND_MIDDLE_PROXIMAL_FB = 30,
	XRT_BODY_JOINT_LEFT_HAND_MIDDLE_INTERMEDIATE_FB = 31,
	XRT_BODY_JOINT_LEFT_HAND_MIDDLE_DISTAL_FB = 32,
	XRT_BODY_JOINT_LEFT_HAND_MIDDLE_TIP_FB = 33,
	XRT_BODY_JOINT_LEFT_HAND_RING_METACARPAL_FB = 34,
	XRT_BODY_JOINT_LEFT_HAND_RING_PROXIMAL_FB = 35,
	XRT_BODY_JOINT_LEFT_HAND_RING_INTERMEDIATE_FB = 36,
	XRT_BODY_JOINT_LEFT_HAND_RING_DISTAL_FB = 37,
	XRT_BODY_JOINT_LEFT_HAND_RING_TIP_FB = 38,
	XRT_BODY_JOINT_LEFT_HAND_LITTLE_METACARPAL_FB = 39,
	XRT_BODY_JOINT_LEFT_HAND_LITTLE_PROXIMAL_FB = 40,
	XRT_BODY_JOINT_LEFT_HAND_LITTLE_INTERMEDIATE_FB = 41,
	XRT_BODY_JOINT_LEFT_HAND_LITTLE_DISTAL_FB = 42,
	XRT_BODY_JOINT_LEFT_HAND_LITTLE_TIP_FB = 43,
	XRT_BODY_JOINT_RIGHT_HAND_PALM_FB = 44,
	XRT_BODY_JOINT_RIGHT_HAND_WRIST_FB = 45,
	XRT_BODY_JOINT_RIGHT_HAND_THUMB_METACARPAL_FB = 46,
	XRT_BODY_JOINT_RIGHT_HAND_THUMB_PROXIMAL_FB = 47,
	XRT_BODY_JOINT_RIGHT_HAND_THUMB_DISTAL_FB = 48,
	XRT_BODY_JOINT_RIGHT_HAND_THUMB_TIP_FB = 49,
	XRT_BODY_JOINT_RIGHT_HAND_INDEX_METACARPAL_FB = 50,
	XRT_BODY_JOINT_RIGHT_HAND_INDEX_PROXIMAL_FB = 51,
	XRT_BODY_JOINT_RIGHT_HAND_INDEX_INTERMEDIATE_FB = 52,
	XRT_BODY_JOINT_RIGHT_HAND_INDEX_DISTAL_FB = 53,
	XRT_BODY_JOINT_RIGHT_HAND_INDEX_TIP_FB = 54,
	XRT_BODY_JOINT_RIGHT_HAND_MIDDLE_METACARPAL_FB = 55,
	XRT_BODY_JOINT_RIGHT_HAND_MIDDLE_PROXIMAL_FB = 56,
	XRT_BODY_JOINT_RIGHT_HAND_MIDDLE_INTERMEDIATE_FB = 57,
	XRT_BODY_JOINT_RIGHT_HAND_MIDDLE_DISTAL_FB = 58,
	XRT_BODY_JOINT_RIGHT_HAND_MIDDLE_TIP_FB = 59,
	XRT_BODY_JOINT_RIGHT_HAND_RING_METACARPAL_FB = 60,
	XRT_BODY_JOINT_RIGHT_HAND_RING_PROXIMAL_FB = 61,
	XRT_BODY_JOINT_RIGHT_HAND_RING_INTERMEDIATE_FB = 62,
	XRT_BODY_JOINT_RIGHT_HAND_RING_DISTAL_FB = 63,
	XRT_BODY_JOINT_RIGHT_HAND_RING_TIP_FB = 64,
	XRT_BODY_JOINT_RIGHT_HAND_LITTLE_METACARPAL_FB = 65,
	XRT_BODY_JOINT_RIGHT_HAND_LITTLE_PROXIMAL_FB = 66,
	XRT_BODY_JOINT_RIGHT_HAND_LITTLE_INTERMEDIATE_FB = 67,
	XRT_BODY_JOINT_RIGHT_HAND_LITTLE_DISTAL_FB = 68,
	XRT_BODY_JOINT_RIGHT_HAND_LITTLE_TIP_FB = 69,
	XRT_BODY_JOINT_COUNT_FB = 70,
	XRT_BODY_JOINT_NONE_FB = -1,
};

// XR_META_body_tracking_full_body
enum xrt_full_body_joint_meta
{
	XRT_FULL_BODY_JOINT_ROOT_META = 0,
	XRT_FULL_BODY_JOINT_HIPS_META = 1,
	XRT_FULL_BODY_JOINT_SPINE_LOWER_META = 2,
	XRT_FULL_BODY_JOINT_SPINE_MIDDLE_META = 3,
	XRT_FULL_BODY_JOINT_SPINE_UPPER_META = 4,
	XRT_FULL_BODY_JOINT_CHEST_META = 5,
	XRT_FULL_BODY_JOINT_NECK_META = 6,
	XRT_FULL_BODY_JOINT_HEAD_META = 7,
	XRT_FULL_BODY_JOINT_LEFT_SHOULDER_META = 8,
	XRT_FULL_BODY_JOINT_LEFT_SCAPULA_META = 9,
	XRT_FULL_BODY_JOINT_LEFT_ARM_UPPER_META = 10,
	XRT_FULL_BODY_JOINT_LEFT_ARM_LOWER_META = 11,
	XRT_FULL_BODY_JOINT_LEFT_HAND_WRIST_TWIST_META = 12,
	XRT_FULL_BODY_JOINT_RIGHT_SHOULDER_META = 13,
	XRT_FULL_BODY_JOINT_RIGHT_SCAPULA_META = 14,
	XRT_FULL_BODY_JOINT_RIGHT_ARM_UPPER_META = 15,
	XRT_FULL_BODY_JOINT_RIGHT_ARM_LOWER_META = 16,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_WRIST_TWIST_META = 17,
	XRT_FULL_BODY_JOINT_LEFT_HAND_PALM_META = 18,
	XRT_FULL_BODY_JOINT_LEFT_HAND_WRIST_META = 19,
	XRT_FULL_BODY_JOINT_LEFT_HAND_THUMB_METACARPAL_META = 20,
	XRT_FULL_BODY_JOINT_LEFT_HAND_THUMB_PROXIMAL_META = 21,
	XRT_FULL_BODY_JOINT_LEFT_HAND_THUMB_DISTAL_META = 22,
	XRT_FULL_BODY_JOINT_LEFT_HAND_THUMB_TIP_META = 23,
	XRT_FULL_BODY_JOINT_LEFT_HAND_INDEX_METACARPAL_META = 24,
	XRT_FULL_BODY_JOINT_LEFT_HAND_INDEX_PROXIMAL_META = 25,
	XRT_FULL_BODY_JOINT_LEFT_HAND_INDEX_INTERMEDIATE_META = 26,
	XRT_FULL_BODY_JOINT_LEFT_HAND_INDEX_DISTAL_META = 27,
	XRT_FULL_BODY_JOINT_LEFT_HAND_INDEX_TIP_META = 28,
	XRT_FULL_BODY_JOINT_LEFT_HAND_MIDDLE_METACARPAL_META = 29,
	XRT_FULL_BODY_JOINT_LEFT_HAND_MIDDLE_PROXIMAL_META = 30,
	XRT_FULL_BODY_JOINT_LEFT_HAND_MIDDLE_INTERMEDIATE_META = 31,
	XRT_FULL_BODY_JOINT_LEFT_HAND_MIDDLE_DISTAL_META = 32,
	XRT_FULL_BODY_JOINT_LEFT_HAND_MIDDLE_TIP_META = 33,
	XRT_FULL_BODY_JOINT_LEFT_HAND_RING_METACARPAL_META = 34,
	XRT_FULL_BODY_JOINT_LEFT_HAND_RING_PROXIMAL_META = 35,
	XRT_FULL_BODY_JOINT_LEFT_HAND_RING_INTERMEDIATE_META = 36,
	XRT_FULL_BODY_JOINT_LEFT_HAND_RING_DISTAL_META = 37,
	XRT_FULL_BODY_JOINT_LEFT_HAND_RING_TIP_META = 38,
	XRT_FULL_BODY_JOINT_LEFT_HAND_LITTLE_METACARPAL_META = 39,
	XRT_FULL_BODY_JOINT_LEFT_HAND_LITTLE_PROXIMAL_META = 40,
	XRT_FULL_BODY_JOINT_LEFT_HAND_LITTLE_INTERMEDIATE_META = 41,
	XRT_FULL_BODY_JOINT_LEFT_HAND_LITTLE_DISTAL_META = 42,
	XRT_FULL_BODY_JOINT_LEFT_HAND_LITTLE_TIP_META = 43,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_PALM_META = 44,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_WRIST_META = 45,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_THUMB_METACARPAL_META = 46,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_THUMB_PROXIMAL_META = 47,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_THUMB_DISTAL_META = 48,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_THUMB_TIP_META = 49,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_INDEX_METACARPAL_META = 50,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_INDEX_PROXIMAL_META = 51,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_INDEX_INTERMEDIATE_META = 52,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_INDEX_DISTAL_META = 53,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_INDEX_TIP_META = 54,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_MIDDLE_METACARPAL_META = 55,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_MIDDLE_PROXIMAL_META = 56,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_MIDDLE_INTERMEDIATE_META = 57,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_MIDDLE_DISTAL_META = 58,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_MIDDLE_TIP_META = 59,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_RING_METACARPAL_META = 60,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_RING_PROXIMAL_META = 61,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_RING_INTERMEDIATE_META = 62,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_RING_DISTAL_META = 63,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_RING_TIP_META = 64,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_LITTLE_METACARPAL_META = 65,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_LITTLE_PROXIMAL_META = 66,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_LITTLE_INTERMEDIATE_META = 67,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_LITTLE_DISTAL_META = 68,
	XRT_FULL_BODY_JOINT_RIGHT_HAND_LITTLE_TIP_META = 69,
	XRT_FULL_BODY_JOINT_LEFT_UPPER_LEG_META = 70,
	XRT_FULL_BODY_JOINT_LEFT_LOWER_LEG_META = 71,
	XRT_FULL_BODY_JOINT_LEFT_FOOT_ANKLE_TWIST_META = 72,
	XRT_FULL_BODY_JOINT_LEFT_FOOT_ANKLE_META = 73,
	XRT_FULL_BODY_JOINT_LEFT_FOOT_SUBTALAR_META = 74,
	XRT_FULL_BODY_JOINT_LEFT_FOOT_TRANSVERSE_META = 75,
	XRT_FULL_BODY_JOINT_LEFT_FOOT_BALL_META = 76,
	XRT_FULL_BODY_JOINT_RIGHT_UPPER_LEG_META = 77,
	XRT_FULL_BODY_JOINT_RIGHT_LOWER_LEG_META = 78,
	XRT_FULL_BODY_JOINT_RIGHT_FOOT_ANKLE_TWIST_META = 79,
	XRT_FULL_BODY_JOINT_RIGHT_FOOT_ANKLE_META = 80,
	XRT_FULL_BODY_JOINT_RIGHT_FOOT_SUBTALAR_META = 81,
	XRT_FULL_BODY_JOINT_RIGHT_FOOT_TRANSVERSE_META = 82,
	XRT_FULL_BODY_JOINT_RIGHT_FOOT_BALL_META = 83,
	XRT_FULL_BODY_JOINT_COUNT_META = 84,
	XRT_FULL_BODY_JOINT_NONE_META = 85,
};

enum xrt_body_joint_set_type_fb
{
	XRT_BODY_JOINT_SET_UNKNOWN = 0,
	// XR_FB_body_tracking
	XRT_BODY_JOINT_SET_DEFAULT_FB,
	// XR_Meta_body_tracking_full_body
	XRT_BODY_JOINT_SET_FULL_BODY_META,
};

// XR_META_body_tracking_fidelity
enum xrt_body_tracking_fidelity_meta
{
	XRT_BODY_TRACKING_FIDELITY_LOW_META = 1,
	XRT_BODY_TRACKING_FIDELITY_HIGH_META = 2,
};

// XR_FB_body_tracking
struct xrt_body_skeleton_joint_fb
{
	struct xrt_pose pose;
	int32_t joint;
	int32_t parent_joint;
};

// XR_FB_body_tracking
struct xrt_body_skeleton_fb
{
	// ordered by xrt_body_joint_fb
	struct xrt_body_skeleton_joint_fb joints[XRT_BODY_JOINT_COUNT_FB];
};

// XR_Meta_body_tracking_full_body
struct xrt_full_body_skeleton_meta
{
	// ordered by xrt_full_body_joint_meta
	struct xrt_body_skeleton_joint_fb joints[XRT_FULL_BODY_JOINT_COUNT_META];
};

// structure is a container to represent the body skeleton in T-pose including the joint hierarchy,
// can have info such as skeleton scale and proportions
struct xrt_body_skeleton
{
	union {
		struct xrt_body_skeleton_fb body_skeleton_fb;
		struct xrt_full_body_skeleton_meta full_body_skeleton_meta;
	};
};

struct xrt_body_joint_location_fb
{
	struct xrt_space_relation relation;
};

struct xrt_base_body_joint_set_meta
{
	uint64_t sample_time_ns;
	float confidence;
	uint32_t skeleton_changed_count;
	bool is_active;

	struct
	{
		// Requires XR_META_body_tracking_fidelity, @ref xrt_device::body_tracking_fidelity_supported
		enum xrt_body_tracking_fidelity_meta fidelity_status;
	} exts;
};

// XR_FB_body_tracking
struct xrt_body_joint_set_fb
{
	struct xrt_base_body_joint_set_meta base;
	// ordered by xrt_body_joint_fb
	struct xrt_body_joint_location_fb joint_locations[XRT_BODY_JOINT_COUNT_FB];
};

// XR_Meta_body_tracking_full_body
struct xrt_full_body_joint_set_meta
{
	struct xrt_base_body_joint_set_meta base;
	// ordered by xrt_full_body_joint_meta
	struct xrt_body_joint_location_fb joint_locations[XRT_FULL_BODY_JOINT_COUNT_META];
};

struct xrt_body_joint_set
{
	union {
		struct xrt_base_body_joint_set_meta base_body_joint_set_meta;
		struct xrt_body_joint_set_fb body_joint_set_fb;
		struct xrt_full_body_joint_set_meta full_body_joint_set_meta;
	};
	// in driver global space, without tracking_origin offset
	struct xrt_space_relation body_pose;
};

/*!
 * Name of a output with a baked in type.
 *
 * @see xrt_output_type
 * @ingroup xrt_iface
 */
enum xrt_output_name
{
	// clang-format off
	XRT_OUTPUT_NAME_SIMPLE_VIBRATION            = XRT_OUTPUT_NAME(0x0010, VIBRATION),
	XRT_OUTPUT_NAME_PSMV_RUMBLE_VIBRATION       = XRT_OUTPUT_NAME(0x0020, VIBRATION),
	XRT_OUTPUT_NAME_INDEX_HAPTIC                = XRT_OUTPUT_NAME(0x0030, VIBRATION),
	XRT_OUTPUT_NAME_VIVE_HAPTIC                 = XRT_OUTPUT_NAME(0x0040, VIBRATION),
	XRT_OUTPUT_NAME_WMR_HAPTIC                  = XRT_OUTPUT_NAME(0x0050, VIBRATION),

	XRT_OUTPUT_NAME_XBOX_HAPTIC_LEFT            = XRT_OUTPUT_NAME(0x0060, VIBRATION),
	XRT_OUTPUT_NAME_XBOX_HAPTIC_RIGHT           = XRT_OUTPUT_NAME(0x0061, VIBRATION),
	XRT_OUTPUT_NAME_XBOX_HAPTIC_LEFT_TRIGGER    = XRT_OUTPUT_NAME(0x0062, VIBRATION),
	XRT_OUTPUT_NAME_XBOX_HAPTIC_RIGHT_TRIGGER   = XRT_OUTPUT_NAME(0x0063, VIBRATION),

	XRT_OUTPUT_NAME_TOUCH_HAPTIC                = XRT_OUTPUT_NAME(0x0070, VIBRATION),

	XRT_OUTPUT_NAME_FORCE_FEEDBACK_LEFT         = XRT_OUTPUT_NAME(0x0080, FORCE_FEEDBACK),
	XRT_OUTPUT_NAME_FORCE_FEEDBACK_RIGHT        = XRT_OUTPUT_NAME(0x0081, FORCE_FEEDBACK),

	XRT_OUTPUT_NAME_G2_CONTROLLER_HAPTIC        = XRT_OUTPUT_NAME(0x0090, VIBRATION),
	XRT_OUTPUT_NAME_ODYSSEY_CONTROLLER_HAPTIC   = XRT_OUTPUT_NAME(0x00A0, VIBRATION),
	XRT_OUTPUT_NAME_ML2_CONTROLLER_VIBRATION    = XRT_OUTPUT_NAME(0x00B0, VIBRATION),

	XRT_OUTPUT_NAME_PSSENSE_VIBRATION           = XRT_OUTPUT_NAME(0x00C0, VIBRATION),
	XRT_OUTPUT_NAME_PSSENSE_TRIGGER_FEEDBACK    = XRT_OUTPUT_NAME(0x00C1, FORCE_FEEDBACK),

	XRT_OUTPUT_NAME_VIVE_TRACKER_HAPTIC         = XRT_OUTPUT_NAME(0x00D0, VIBRATION),

	XRT_OUTPUT_NAME_OPPO_MR_HAPTIC              = XRT_OUTPUT_NAME(0x00E0, VIBRATION),
	// clang-format on
};

/*!
 * Value used to indicate a haptic pulse of the minimal supported duration.
 *
 * @ingroup xrt_iface
 */
#define XRT_MIN_HAPTIC_DURATION -1

/*!
 * Value used to indicate a haptic pulse of some runtime defined optimal
 * frequency.
 *
 * @ingroup xrt_iface
 */

#define XRT_FREQUENCY_UNSPECIFIED 0

/*!
 * Value used as a timeout to indicate the timeout should never occur.
 *
 * @ingroup xrt_iface
 */
#define XRT_INFINITE_DURATION (0x7fffffffffffffffLL)

enum xrt_force_feedback_location
{
	XRT_FORCE_FEEDBACK_LOCATION_LEFT_THUMB,
	XRT_FORCE_FEEDBACK_LOCATION_LEFT_INDEX,
	XRT_FORCE_FEEDBACK_LOCATION_LEFT_MIDDLE,
	XRT_FORCE_FEEDBACK_LOCATION_LEFT_RING,
	XRT_FORCE_FEEDBACK_LOCATION_LEFT_PINKY,
};

struct xrt_output_force_feedback
{
	float value;
	enum xrt_force_feedback_location location;
};

/*!
 * A union of all output types.
 *
 * @see xrt_output_type
 * @ingroup xrt_iface math
 */
union xrt_output_value {
	struct
	{
		float frequency;
		float amplitude;
		int64_t duration_ns;
	} vibration;

	struct
	{
		struct xrt_output_force_feedback force_feedback[5];
		uint64_t force_feedback_location_count;
	} force_feedback;
};


/*
 *
 * Misc enums.
 *
 */

/*!
 * What form factor is this device, mostly maps onto OpenXR's @p XrFormFactor.
 *
 * @ingroup xrt_iface
 */
enum xrt_form_factor
{
	XRT_FORM_FACTOR_HMD,      //!< Head mounted display.
	XRT_FORM_FACTOR_HANDHELD, //!< Handheld display.
};

/*!
 * Domain type.
 * Use for performance level setting
 * Which hardware should be boost/decrease
 */
enum xrt_perf_domain
{
	XRT_PERF_DOMAIN_CPU = 1,
	XRT_PERF_DOMAIN_GPU = 2,
};

enum xrt_perf_sub_domain
{
	XRT_PERF_SUB_DOMAIN_COMPOSITING = 1,
	XRT_PERF_SUB_DOMAIN_RENDERING = 2,
	XRT_PERF_SUB_DOMAIN_THERMAL = 3
};

/*!
 * Performance level.
 */
enum xrt_perf_set_level
{
	XRT_PERF_SET_LEVEL_POWER_SAVINGS = 0,
	XRT_PERF_SET_LEVEL_SUSTAINED_LOW = 25,
	XRT_PERF_SET_LEVEL_SUSTAINED_HIGH = 50,
	XRT_PERF_SET_LEVEL_BOOST = 75,
};

/*!
 * Performance level.
 */
enum xrt_perf_notify_level
{
	XRT_PERF_NOTIFY_LEVEL_NORMAL = 0,
	XRT_PERF_NOTIFY_LEVEL_WARNING = 25,
	XRT_PERF_NOTIFY_LEVEL_IMPAIRED = 75,
};

/*!
 * Visibility mask, mirror of XrVisibilityMaskKHR
 *
 * @ingroup xrt_iface
 */
enum xrt_visibility_mask_type
{
	XRT_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH = 1,
	XRT_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH = 2,
	XRT_VISIBILITY_MASK_TYPE_LINE_LOOP = 3,
};


/*
 *
 * Inline functions
 *
 */

/*!
 * Increment the reference, probably want @ref xrt_reference_inc_and_was_zero.
 *
 * @memberof xrt_reference
 * @ingroup xrt_iface
 */
static inline void
xrt_reference_inc(struct xrt_reference *xref)
{
	xrt_atomic_s32_inc_return(&xref->count);
}

/*!
 * Decrement the reference, probably want @ref xrt_reference_dec_and_is_zero.
 *
 * @memberof xrt_reference
 * @ingroup xrt_iface
 */
static inline void
xrt_reference_dec(struct xrt_reference *xref)
{
	xrt_atomic_s32_dec_return(&xref->count);
}

/*!
 * Increment the reference and return true if the value @p was zero.
 *
 * @memberof xrt_reference
 * @ingroup xrt_iface
 */
XRT_CHECK_RESULT static inline bool
xrt_reference_inc_and_was_zero(struct xrt_reference *xref)
{
	int32_t count = xrt_atomic_s32_inc_return(&xref->count);
	return count == 1;
}

/*!
 * Decrement the reference and return true if the value is now zero.
 *
 * @memberof xrt_reference
 * @ingroup xrt_iface
 */
XRT_CHECK_RESULT static inline bool
xrt_reference_dec_and_is_zero(struct xrt_reference *xref)
{
	int32_t count = xrt_atomic_s32_dec_return(&xref->count);
	return count == 0;
}


#ifdef __cplusplus
}
#endif

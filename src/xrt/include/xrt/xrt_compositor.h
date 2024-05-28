// Copyright 2019-2024, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Header declaring XRT graphics interfaces.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @author Christoph Haag <christoph.haag@collabora.com>
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt/xrt_limits.h"
#include "xrt/xrt_defines.h"
#include "xrt/xrt_handles.h"
#include "xrt/xrt_config_os.h"
#include "xrt/xrt_config_have.h"
#include "xrt/xrt_windows.h"

#if defined(XRT_HAVE_D3D11)
#include <d3d11.h>
#elif defined(XRT_DOXYGEN)
struct ID3D11Texture2D;
#endif

#if defined(XRT_HAVE_D3D12)
#include <d3d12.h>
#elif defined(XRT_DOXYGEN)
struct ID3D12Resource;
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*
 *
 * Pre-declare things, also they should not be in the xrt_iface group.
 *
 */

struct xrt_device;
struct xrt_image_native;
struct xrt_compositor;
struct xrt_session_event_sink;

typedef struct VkCommandBuffer_T *VkCommandBuffer;
#ifdef XRT_64_BIT
typedef struct VkImage_T *VkImage;
typedef struct VkDeviceMemory_T *VkDeviceMemory;
#else
typedef uint64_t VkImage;
typedef uint64_t VkDeviceMemory;
#endif


/*!
 * @addtogroup xrt_iface
 * @{
 */


/*
 *
 * Layers.
 *
 */

/*!
 * Layer type.
 */
enum xrt_layer_type
{
	XRT_LAYER_PROJECTION,
	XRT_LAYER_PROJECTION_DEPTH,
	XRT_LAYER_QUAD,
	XRT_LAYER_CUBE,
	XRT_LAYER_CYLINDER,
	XRT_LAYER_EQUIRECT1,
	XRT_LAYER_EQUIRECT2,
	XRT_LAYER_PASSTHROUGH
};

/*!
 * Bit field for holding information about how a layer should be composited.
 */
enum xrt_layer_composition_flags
{
	XRT_LAYER_COMPOSITION_CORRECT_CHROMATIC_ABERRATION_BIT = 1u << 0u,
	XRT_LAYER_COMPOSITION_BLEND_TEXTURE_SOURCE_ALPHA_BIT = 1u << 1u,
	XRT_LAYER_COMPOSITION_UNPREMULTIPLIED_ALPHA_BIT = 1u << 2u,
	/*!
	 * The layer is locked to the device and the pose should only be
	 * adjusted for the IPD.
	 */
	XRT_LAYER_COMPOSITION_VIEW_SPACE_BIT = 1u << 3u,

	/*!
	 * If this flag is set the compositor should use the scale and bias
	 * from the @ref xrt_layer_data struct.
	 */
	XRT_LAYER_COMPOSITION_COLOR_BIAS_SCALE = 1u << 4u,

	//! Normal super sampling, see XrCompositionLayerSettingsFlagsFB.
	XRT_COMPOSITION_LAYER_PROCESSING_NORMAL_SUPER_SAMPLING_BIT_FB = 1u << 5u,

	//! Quality super sampling, see XrCompositionLayerSettingsFlagsFB.
	XRT_COMPOSITION_LAYER_PROCESSING_QUALITY_SUPER_SAMPLING_BIT_FB = 1u << 6u,

	//! Normal sharpening, see XrCompositionLayerSettingsFlagsFB.
	XRT_COMPOSITION_LAYER_PROCESSING_NORMAL_SHARPENING_BIT_FB = 1u << 7u,

	//! Quality sharpening, see XrCompositionLayerSettingsFlagsFB.
	XRT_COMPOSITION_LAYER_PROCESSING_QUALITY_SHARPENING_BIT_FB = 1u << 8u,

	/*!
	 * This layer has advanced blending information, this bit
	 * supersedes the behavior of
	 * @ref XRT_LAYER_COMPOSITION_BLEND_TEXTURE_SOURCE_ALPHA_BIT,
	 * see @p XrCompositionLayerAlphaBlendFB.
	 */
	XRT_LAYER_COMPOSITION_ADVANCED_BLENDING_BIT = 1u << 9u,

	/*!
	 * Depth testing is requested when composing this layer if this flag is set,
	 * see XrCompositionLayerDepthTestFB.
	 */
	XRT_LAYER_COMPOSITION_DEPTH_TEST = 1u << 10u,
};

/*!
 * XrCompareOpFB
 */
enum xrt_compare_op_fb
{
	XRT_COMPARE_OP_NEVER_FB = 0,
	XRT_COMPARE_OP_LESS_FB = 1,
	XRT_COMPARE_OP_EQUAL_FB = 2,
	XRT_COMPARE_OP_LESS_OR_EQUAL_FB = 3,
	XRT_COMPARE_OP_GREATER_FB = 4,
	XRT_COMPARE_OP_NOT_EQUAL_FB = 5,
	XRT_COMPARE_OP_GREATER_OR_EQUAL_FB = 6,
	XRT_COMPARE_OP_ALWAYS_FB = 7,
	XRT_COMPARE_OP_MAX_ENUM_FB = 0x7FFFFFFF
};

/*!
 * Which view is the layer visible to?
 *
 * Used for quad layers.
 *
 * @note Doesn't have the same values as the OpenXR counterpart!
 */
enum xrt_layer_eye_visibility
{
	XRT_LAYER_EYE_VISIBILITY_NONE = 0x0,
	XRT_LAYER_EYE_VISIBILITY_LEFT_BIT = 0x1,
	XRT_LAYER_EYE_VISIBILITY_RIGHT_BIT = 0x2,
	XRT_LAYER_EYE_VISIBILITY_BOTH = 0x3,
};

/*!
 * Blend factors.
 */
enum xrt_blend_factor
{
	XRT_BLEND_FACTOR_ZERO = 0,
	XRT_BLEND_FACTOR_ONE = 1,
	XRT_BLEND_FACTOR_SRC_ALPHA = 2,
	XRT_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 3,
	XRT_BLEND_FACTOR_DST_ALPHA = 4,
	XRT_BLEND_FACTOR_ONE_MINUS_DST_ALPHA = 5,
	XRT_BLEND_FACTOR_MAX_ENUM_FB = 0x7FFFFFFF,
};

/*!
 * Advanced blend
 * provides explicit control over source and destination blend factors,
 * with separate controls for color and alpha
 *
 * See @ref XRT_LAYER_COMPOSITION_ADVANCED_BLENDING_BIT.
 */
struct xrt_layer_advanced_blend_data
{
	enum xrt_blend_factor src_factor_color;
	enum xrt_blend_factor dst_factor_color;
	enum xrt_blend_factor src_factor_alpha;
	enum xrt_blend_factor dst_factor_alpha;
};

/*!
 * Specifies a sub-image in a layer.
 */
struct xrt_sub_image
{
	//! Image index in the (implicit) swapchain
	uint32_t image_index;
	//! Index in image array (for array textures)
	uint32_t array_index;
	//! The rectangle in the image to use
	struct xrt_rect rect;
	//! Normalized sub image coordinates and size.
	struct xrt_normalized_rect norm_rect;
};

/*!
 * All of the pure data values associated with a single view in a projection
 * layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_projection_view_data
{
	struct xrt_sub_image sub;

	struct xrt_fov fov;
	struct xrt_pose pose;
};

/*!
 * All the pure data values associated with a projection layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_projection_data
{
	struct xrt_layer_projection_view_data v[XRT_MAX_VIEWS];
};

/*!
 * All the pure data values associated with a depth information attached
 * to a layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_depth_data
{
	struct xrt_sub_image sub;

	float min_depth;
	float max_depth;
	float near_z;
	float far_z;
};

struct xrt_layer_depth_test_data
{
	bool depth_mask;
	enum xrt_compare_op_fb compare_op;
};

/*!
 * All the pure data values associated with a projection layer with depth
 * swapchain attached.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_projection_depth_data
{
	struct xrt_layer_projection_view_data v[XRT_MAX_VIEWS];

	struct xrt_layer_depth_data d[XRT_MAX_VIEWS];
};

/*!
 * All the pure data values associated with a quad layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_quad_data
{
	enum xrt_layer_eye_visibility visibility;

	struct xrt_sub_image sub;

	struct xrt_pose pose;
	struct xrt_vec2 size;
};

/*!
 * All the pure data values associated with a cube layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_cube_data
{
	enum xrt_layer_eye_visibility visibility;

	struct xrt_sub_image sub;

	struct xrt_pose pose;
};

/*!
 * All the pure data values associated with a cylinder layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_cylinder_data
{
	enum xrt_layer_eye_visibility visibility;

	struct xrt_sub_image sub;

	struct xrt_pose pose;
	float radius;
	float central_angle;
	float aspect_ratio;
};

/*!
 * All the pure data values associated with a equirect1 layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_equirect1_data
{
	enum xrt_layer_eye_visibility visibility;

	struct xrt_sub_image sub;

	struct xrt_pose pose;
	float radius;
	struct xrt_vec2 scale;
	struct xrt_vec2 bias;
};

/*!
 * All the pure data values associated with a equirect2 layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_equirect2_data
{
	enum xrt_layer_eye_visibility visibility;

	struct xrt_sub_image sub;

	struct xrt_pose pose;
	float radius;
	float central_horizontal_angle;
	float upper_vertical_angle;
	float lower_vertical_angle;
};

/*!
 * @interface xrt_passthrough
 */
struct xrt_passthrough
{
	bool paused;
};

/*!
 * @interface xrt_passthrough_layer
 */
struct xrt_passthrough_layer
{
	bool paused;
};

/*!
 * All the pure data values associated with a passthrough layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_passthrough_data
{
	struct xrt_passthrough xrt_pt;
	struct xrt_passthrough_layer xrt_pl;
};

/*!
 * All the pure data values associated with a composition layer.
 *
 * The @ref xrt_swapchain references and @ref xrt_device are provided outside of
 * this struct.
 */
struct xrt_layer_data
{
	/*!
	 * Tag for compositor layer type.
	 */
	enum xrt_layer_type type;

	/*!
	 * Often @ref XRT_INPUT_GENERIC_HEAD_POSE
	 */
	enum xrt_input_name name;

	/*!
	 * "Display no-earlier-than" timestamp for this layer.
	 *
	 * The layer may be displayed after this point, but must never be
	 * displayed before.
	 */
	uint64_t timestamp;

	/*!
	 * Composition flags
	 */
	enum xrt_layer_composition_flags flags;

	/*!
	 * Depth test data
	 */
	struct xrt_layer_depth_test_data depth_test;

	/*!
	 * Whether the main compositor should flip the direction of y when
	 * rendering.
	 *
	 * This is actually an input only to the "main" compositor
	 * comp_compositor. It is overwritten by the various client
	 * implementations of the @ref xrt_compositor interface depending on the
	 * conventions of the associated graphics API. Other @ref
	 * xrt_compositor_native implementations that are not the main
	 * compositor just pass this field along unchanged to the "real"
	 * compositor.
	 */
	bool flip_y;

	/*!
	 * Modulate the color sourced from the images.
	 */
	struct xrt_colour_rgba_f32 color_scale;

	/*!
	 * Modulate the color sourced from the images.
	 */
	struct xrt_colour_rgba_f32 color_bias;

	/*!
	 * Advanced blend factors
	 */
	struct xrt_layer_advanced_blend_data advanced_blend;

	/*!
	 * Union of data values for the various layer types.
	 *
	 * The initialized member of this union should match the value of
	 * xrt_layer_data::type. It also should be clear because of the layer
	 * function called between xrt_compositor::layer_begin and
	 * xrt_compositor::layer_commit where this data was passed.
	 */
	union {
		struct xrt_layer_projection_data proj;
		struct xrt_layer_projection_depth_data depth;
		struct xrt_layer_quad_data quad;
		struct xrt_layer_cube_data cube;
		struct xrt_layer_cylinder_data cylinder;
		struct xrt_layer_equirect1_data equirect1;
		struct xrt_layer_equirect2_data equirect2;
		struct xrt_layer_passthrough_data passthrough;
	};
	uint32_t view_count;
};

/*!
 * Per frame data for the layer submission calls, used in
 * @ref xrt_compositor::layer_begin.
 */
struct xrt_layer_frame_data
{
	int64_t frame_id;
	uint64_t display_time_ns;
	enum xrt_blend_mode env_blend_mode;
};


/*
 *
 * Swapchain.
 *
 */

/*!
 * Special flags for creating swapchain images.
 */
enum xrt_swapchain_create_flags
{
	//! Our compositor just ignores this bit.
	XRT_SWAPCHAIN_CREATE_PROTECTED_CONTENT = (1u << 0u),
	//! Signals that the allocator should only allocate one image.
	XRT_SWAPCHAIN_CREATE_STATIC_IMAGE = (1u << 1u),
};

/*!
 * Usage of the swapchain images.
 */
enum xrt_swapchain_usage_bits
{
	XRT_SWAPCHAIN_USAGE_COLOR = 0x00000001,
	XRT_SWAPCHAIN_USAGE_DEPTH_STENCIL = 0x00000002,
	XRT_SWAPCHAIN_USAGE_UNORDERED_ACCESS = 0x00000004,
	XRT_SWAPCHAIN_USAGE_TRANSFER_SRC = 0x00000008,
	XRT_SWAPCHAIN_USAGE_TRANSFER_DST = 0x00000010,
	XRT_SWAPCHAIN_USAGE_SAMPLED = 0x00000020,
	XRT_SWAPCHAIN_USAGE_MUTABLE_FORMAT = 0x00000040,
	XRT_SWAPCHAIN_USAGE_INPUT_ATTACHMENT = 0x00000080,
};

/*!
 * The direction of a transition.
 */
enum xrt_barrier_direction
{
	XRT_BARRIER_TO_APP = 1,
	XRT_BARRIER_TO_COMP = 2,
};

/*!
 * @interface xrt_swapchain
 *
 * Common swapchain interface/base.
 *
 * Swapchains are owned by the @ref xrt_compositor that they where created from,
 * it's the state trackers job to ensure all swapchains are destroyed before
 * destroying the @ref xrt_compositor.
 */
struct xrt_swapchain
{
	/*!
	 * Reference helper.
	 */
	struct xrt_reference reference;

	/*!
	 * Number of images.
	 *
	 * The images themselves are on the subclasses.
	 */
	uint32_t image_count;

	/*!
	 * @ref dec_image_use must have been called as often as @ref inc_image_use.
	 */
	void (*destroy)(struct xrt_swapchain *xsc);

	/*!
	 * Obtain the index of the next image to use, without blocking on being
	 * able to write to it.
	 *
	 * See xrAcquireSwapchainImage.
	 *
	 * Caller must make sure that no image is acquired before calling
	 * @ref xrt_swapchain_acquire_image.
	 *
	 * @param xsc Self pointer
	 * @param[out] out_index Image index to use next
	 *
	 * Call @ref xrt_swapchain_wait_image before writing to the image index output from this function.
	 */
	xrt_result_t (*acquire_image)(struct xrt_swapchain *xsc, uint32_t *out_index);

	/*!
	 * @brief Increments the use counter of a swapchain image.
	 */
	xrt_result_t (*inc_image_use)(struct xrt_swapchain *xsc, uint32_t index);

	/*!
	 * @brief Decrements the use counter of a swapchain image.
	 *
	 * @ref wait_image will return once the image use counter is 0.
	 */
	xrt_result_t (*dec_image_use)(struct xrt_swapchain *xsc, uint32_t index);

	/*!
	 * Wait until image @p index is available for exclusive use, or until @p timeout_ns expires.
	 *
	 * See xrWaitSwapchainImage, which is the basis for this API.
	 * The state tracker needs to track image index, which should have come from @ref xrt_swapchain_acquire_image

	 * @param xsc Self pointer
	 * @param timeout_ns Timeout in nanoseconds,
	 * @param index Image index to wait for.
	 */
	xrt_result_t (*wait_image)(struct xrt_swapchain *xsc, uint64_t timeout_ns, uint32_t index);

	/*!
	 * Do any barrier transitions to and from the application.
	 *
	 * @param xsc       Self pointer
	 * @param direction Direction of the barrier transition.
	 * @param index     Image index to barrier transition.
	 */
	xrt_result_t (*barrier_image)(struct xrt_swapchain *xsc, enum xrt_barrier_direction direction, uint32_t index);

	/*!
	 * See xrReleaseSwapchainImage, state tracker needs to track index.
	 */
	xrt_result_t (*release_image)(struct xrt_swapchain *xsc, uint32_t index);
};

/*!
 * Update the reference counts on swapchain(s).
 *
 * @param[in,out] dst Pointer to a object reference: if the object reference is
 *                non-null will decrement its counter. The reference that
 *                @p dst points to will be set to @p src.
 * @param[in] src New object for @p dst to refer to (may be null).
 *                If non-null, will have its refcount increased.
 * @ingroup xrt_iface
 * @relates xrt_swapchain
 */
static inline void
xrt_swapchain_reference(struct xrt_swapchain **dst, struct xrt_swapchain *src)
{
	struct xrt_swapchain *old_dst = *dst;

	if (old_dst == src) {
		return;
	}

	if (src) {
		xrt_reference_inc(&src->reference);
	}

	*dst = src;

	if (old_dst) {
		if (xrt_reference_dec_and_is_zero(&old_dst->reference)) {
			old_dst->destroy(old_dst);
		}
	}
}

/*!
 * @copydoc xrt_swapchain::acquire_image
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_swapchain
 */
static inline xrt_result_t
xrt_swapchain_acquire_image(struct xrt_swapchain *xsc, uint32_t *out_index)
{
	return xsc->acquire_image(xsc, out_index);
}

/*!
 * @copydoc xrt_swapchain::inc_image_use
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_swapchain
 */
static inline xrt_result_t
xrt_swapchain_inc_image_use(struct xrt_swapchain *xsc, uint32_t index)
{
	return xsc->inc_image_use(xsc, index);
}

/*!
 * @copydoc xrt_swapchain::dec_image_use
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_swapchain
 */
static inline xrt_result_t
xrt_swapchain_dec_image_use(struct xrt_swapchain *xsc, uint32_t index)
{
	return xsc->dec_image_use(xsc, index);
}

/*!
 * @copydoc xrt_swapchain::wait_image
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_swapchain
 */
static inline xrt_result_t
xrt_swapchain_wait_image(struct xrt_swapchain *xsc, uint64_t timeout_ns, uint32_t index)
{
	return xsc->wait_image(xsc, timeout_ns, index);
}

/*!
 * @copydoc xrt_swapchain::barrier_image
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_swapchain
 */
static inline xrt_result_t
xrt_swapchain_barrier_image(struct xrt_swapchain *xsc, enum xrt_barrier_direction direction, uint32_t index)
{
	return xsc->barrier_image(xsc, direction, index);
}

/*!
 * @copydoc xrt_swapchain::release_image
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_swapchain
 */
static inline xrt_result_t
xrt_swapchain_release_image(struct xrt_swapchain *xsc, uint32_t index)
{
	return xsc->release_image(xsc, index);
}


/*
 *
 * Fence.
 *
 */

/*!
 * Compositor fence used for syncornization.
 */
struct xrt_compositor_fence
{
	/*!
	 * Waits on the fence with the given timeout.
	 */
	xrt_result_t (*wait)(struct xrt_compositor_fence *xcf, uint64_t timeout);

	/*!
	 * Destroys the fence.
	 */
	void (*destroy)(struct xrt_compositor_fence *xcf);
};

/*!
 * @copydoc xrt_compositor_fence::wait
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor_fence
 */
static inline xrt_result_t
xrt_compositor_fence_wait(struct xrt_compositor_fence *xcf, uint64_t timeout)
{
	return xcf->wait(xcf, timeout);
}

/*!
 * @copydoc xrt_compositor_fence::destroy
 *
 * Helper for calling through the function pointer: does a null check and sets
 * xcf_ptr to null if freed.
 *
 * @public @memberof xrt_compositor_fence
 */
static inline void
xrt_compositor_fence_destroy(struct xrt_compositor_fence **xcf_ptr)
{
	struct xrt_compositor_fence *xcf = *xcf_ptr;
	if (xcf == NULL) {
		return;
	}

	xcf->destroy(xcf);
	*xcf_ptr = NULL;
}


/*
 *
 * Compositor semaphore.
 *
 */

/*!
 * Compositor semaphore used for synchronization, needs to be as capable as a
 * Vulkan pipeline semaphore.
 */
struct xrt_compositor_semaphore
{
	/*!
	 * Reference helper.
	 */
	struct xrt_reference reference;

	/*!
	 * Does a CPU side wait on the semaphore to reach the given value.
	 */
	xrt_result_t (*wait)(struct xrt_compositor_semaphore *xcsem, uint64_t value, uint64_t timeout_ns);

	/*!
	 * Destroys the semaphore.
	 */
	void (*destroy)(struct xrt_compositor_semaphore *xcsem);
};

/*!
 * Update the reference counts on compositor semaphore(s).
 *
 * @param[in,out] dst Pointer to a object reference: if the object reference is
 *                non-null will decrement its counter. The reference that
 *                @p dst points to will be set to @p src.
 * @param[in] src New object for @p dst to refer to (may be null).
 *                If non-null, will have its refcount increased.
 * @ingroup xrt_iface
 * @relates xrt_compositor_semaphore
 */
static inline void
xrt_compositor_semaphore_reference(struct xrt_compositor_semaphore **dst, struct xrt_compositor_semaphore *src)
{
	struct xrt_compositor_semaphore *old_dst = *dst;

	if (old_dst == src) {
		return;
	}

	if (src) {
		xrt_reference_inc(&src->reference);
	}

	*dst = src;

	if (old_dst) {
		if (xrt_reference_dec_and_is_zero(&old_dst->reference)) {
			old_dst->destroy(old_dst);
		}
	}
}

/*!
 * @copydoc xrt_compositor_semaphore::wait
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor_semaphore
 */
static inline xrt_result_t
xrt_compositor_semaphore_wait(struct xrt_compositor_semaphore *xcsem, uint64_t value, uint64_t timeout)
{
	return xcsem->wait(xcsem, value, timeout);
}


/*
 *
 * Compositor.
 *
 */

/*!
 * View type to be rendered to by the compositor.
 */
enum xrt_view_type
{
	XRT_VIEW_TYPE_MONO = 1,
	XRT_VIEW_TYPE_STEREO = 2,
};

enum xrt_compositor_frame_point
{
	XRT_COMPOSITOR_FRAME_POINT_WOKE, //!< The client woke up after waiting.
};

/*!
 * Swapchain creation info.
 */
struct xrt_swapchain_create_info
{
	enum xrt_swapchain_create_flags create;
	enum xrt_swapchain_usage_bits bits;
	uint32_t format;
	uint32_t sample_count;
	uint32_t width;
	uint32_t height;
	uint32_t face_count;
	uint32_t array_size;
	uint32_t mip_count;

	/*
	 * List of formats that could be used when creating views of the swapchain images.
	 * See XR_KHR_vulkan_swapchain_format_list and VK_KHR_image_format_list
	 */
	uint32_t format_count;
	uint32_t formats[XRT_MAX_SWAPCHAIN_CREATE_INFO_FORMAT_LIST_COUNT];
};

/*!
 * Passthrough creation info.
 */
struct xrt_passthrough_create_info
{
	enum xrt_passthrough_create_flags create;
};

/*!
 * Passthrough layer creation info.
 */
struct xrt_passthrough_layer_create_info
{
	enum xrt_passthrough_create_flags create;
	enum xrt_passthrough_purpose_flags purpose;
};

/*!
 * Struct used to negotiate properties of a swapchain that is created outside
 * of the compositor. Often used by a client compositor or IPC layer to allocate
 * the swapchain images and then pass them into the native compositor.
 */
struct xrt_swapchain_create_properties
{
	//! How many images the compositor want in the swapchain.
	uint32_t image_count;

	//! New creation bits.
	enum xrt_swapchain_usage_bits extra_bits;
};

/*!
 * Session information, mostly overlay extension data.
 */
struct xrt_session_info
{
	bool is_overlay;
	uint64_t flags;
	uint32_t z_order;
};

/*!
 * Capabilities and information about the compositor and device together.
 *
 * For client compositors the formats of the native compositor are translated.
 */
struct xrt_compositor_info
{
	//! Number of formats, never changes.
	uint32_t format_count;

	//! Supported formats, never changes.
	int64_t formats[XRT_MAX_SWAPCHAIN_FORMATS];

	//! Max texture size that GPU supports (size of a single dimension), zero means any size.
	uint32_t max_texture_size;
};

/*!
 * Begin Session information not known until clients have created an xrt-instance such as which
 * extensions are enabled, view type, etc.
 */
struct xrt_begin_session_info
{
	enum xrt_view_type view_type;
	bool ext_hand_tracking_enabled;
	bool ext_eye_gaze_interaction_enabled;
	bool ext_hand_interaction_enabled;
	bool htc_facial_tracking_enabled;
	bool fb_body_tracking_enabled;
	bool meta_body_tracking_full_body_enabled;
	bool meta_body_tracking_fidelity_enabled;
	bool meta_body_tracking_calibration_enabled;
};

/*!
 * Hints the XR runtime what type of task the thread is doing.
 */
enum xrt_thread_hint
{
	XRT_THREAD_HINT_APPLICATION_MAIN = 1,
	XRT_THREAD_HINT_APPLICATION_WORKER = 2,
	XRT_THREAD_HINT_RENDERER_MAIN = 3,
	XRT_THREAD_HINT_RENDERER_WORKER = 4,
};

/*!
 * @interface xrt_compositor
 *
 * Common compositor client interface/base.
 *
 * A compositor is very much analogous to a `XrSession` but without any of the
 * input functionality, and does have the same life time as a `XrSession`.
 */
struct xrt_compositor
{
	/*!
	 * Capabilities and recommended values information.
	 */
	struct xrt_compositor_info info;

	/*!
	 * For a given @ref xrt_swapchain_create_info struct returns a filled
	 * out @ref xrt_swapchain_create_properties.
	 */
	xrt_result_t (*get_swapchain_create_properties)(struct xrt_compositor *xc,
	                                                const struct xrt_swapchain_create_info *info,
	                                                struct xrt_swapchain_create_properties *xsccp);

	/*!
	 * @name Function pointers for swapchain and sync creation and import
	 * @{
	 */
	/*!
	 * Create a swapchain with a set of images.
	 *
	 * The pointer pointed to by @p out_xsc has to either be NULL or a valid
	 * @ref xrt_swapchain pointer. If there is a valid @ref xrt_swapchain
	 * pointed by the pointed pointer it will have it reference decremented.
	 */
	xrt_result_t (*create_swapchain)(struct xrt_compositor *xc,
	                                 const struct xrt_swapchain_create_info *info,
	                                 struct xrt_swapchain **out_xsc);

	/*!
	 * Create a swapchain from a set of native images.
	 *
	 * The pointer pointed to by @p out_xsc has to either be NULL or a valid
	 * @ref xrt_swapchain pointer. If there is a valid @ref xrt_swapchain
	 * pointed by the pointed pointer it will have it reference decremented.
	 */
	xrt_result_t (*import_swapchain)(struct xrt_compositor *xc,
	                                 const struct xrt_swapchain_create_info *info,
	                                 struct xrt_image_native *native_images,
	                                 uint32_t image_count,
	                                 struct xrt_swapchain **out_xsc);

	/*!
	 * Create a compositor fence from a native sync handle.
	 */
	xrt_result_t (*import_fence)(struct xrt_compositor *xc,
	                             xrt_graphics_sync_handle_t handle,
	                             struct xrt_compositor_fence **out_xcf);

	/*!
	 * Create a compositor semaphore, also returns a native handle.
	 */
	xrt_result_t (*create_semaphore)(struct xrt_compositor *xc,
	                                 xrt_graphics_sync_handle_t *out_handle,
	                                 struct xrt_compositor_semaphore **out_xcsem);
	/*! @} */

	/*!
	 * Create a passthrough.
	 */
	xrt_result_t (*create_passthrough)(struct xrt_compositor *xc, const struct xrt_passthrough_create_info *info);


	/*!
	 * Create a passthrough layer.
	 */
	xrt_result_t (*create_passthrough_layer)(struct xrt_compositor *xc,
	                                         const struct xrt_passthrough_layer_create_info *info);
	/*!
	 * Destroy a passthrough.
	 */
	xrt_result_t (*destroy_passthrough)(struct xrt_compositor *xc);

	/*!
	 * @name Function pointers for session functions
	 * @{
	 */
	/*!
	 * See xrBeginSession.
	 */
	xrt_result_t (*begin_session)(struct xrt_compositor *xc, const struct xrt_begin_session_info *info);

	/*!
	 * See xrEndSession, unlike the OpenXR one the state tracker is
	 * responsible to call discard frame before calling this function. See
	 * discard_frame.
	 */
	xrt_result_t (*end_session)(struct xrt_compositor *xc);

	/*! @} */

	/*!
	 * @name Function pointers for frame functions
	 * @{
	 */

	/*!
	 * This function and @ref mark_frame function calls are a alternative to
	 * @ref wait_frame.
	 *
	 * The only requirement on the compositor for the @p frame_id
	 * is that it is a positive number and larger then the last returned
	 * frame_id.
	 *
	 * After a call to predict_frame, the state tracker is not allowed to
	 * call this function until after a call to @ref mark_frame (with point
	 * @ref XRT_COMPOSITOR_FRAME_POINT_WOKE), followed by either
	 * @ref begin_frame or @ref discard_frame.
	 *
	 * @param[in]  xc                              The compositor
	 * @param[out] out_frame_id                    Frame id
	 * @param[out] out_wake_time_ns                When we want the client to be awoken to begin rendering.
	 * @param[out] out_predicted_gpu_time_ns       When we expect the client to finish the GPU work. If not
	 *                                             computed/available, set to 0.
	 * @param[out] out_predicted_display_time_ns   When the pixels turns into photons.
	 * @param[out] out_predicted_display_period_ns The period for the frames.
	 */
	xrt_result_t (*predict_frame)(struct xrt_compositor *xc,
	                              int64_t *out_frame_id,
	                              uint64_t *out_wake_time_ns,
	                              uint64_t *out_predicted_gpu_time_ns,
	                              uint64_t *out_predicted_display_time_ns,
	                              uint64_t *out_predicted_display_period_ns);

	/*!
	 * This function and @ref predict_frame function calls are a alternative to
	 * @ref wait_frame.
	 *
	 * If point is @ref XRT_COMPOSITOR_FRAME_POINT_WOKE it is to mark that the
	 * client woke up from waiting on a frame.
	 *
	 * @param[in] xc       The compositor
	 * @param[in] frame_id Frame id
	 * @param[in] point    What type of frame point to mark.
	 * @param[in] when_ns  When this point happened.
	 */
	xrt_result_t (*mark_frame)(struct xrt_compositor *xc,
	                           int64_t frame_id,
	                           enum xrt_compositor_frame_point point,
	                           uint64_t when_ns);

	/*!
	 * See xrWaitFrame.
	 *
	 * This function has the same semantics as calling @ref predict_frame,
	 * sleeping, and then calling @ref mark_frame with a point of
	 * @ref XRT_COMPOSITOR_FRAME_POINT_WOKE.
	 *
	 * The only requirement on the compositor for the @p frame_id
	 * is that it is a positive number and larger then the last returned
	 * @p frame_id.
	 *
	 * After a call to wait_frame, the state tracker is not allowed to call
	 * this function until after a call to either @ref begin_frame or
	 * @ref discard_frame.
	 *
	 * If the caller can do its own blocking, use the pair of functions
	 * xrt_compositor::predict_frame and xrt_compositor::mark_frame instead
	 * of this single blocking function.
	 */
	xrt_result_t (*wait_frame)(struct xrt_compositor *xc,
	                           int64_t *out_frame_id,
	                           uint64_t *out_predicted_display_time,
	                           uint64_t *out_predicted_display_period);

	/*!
	 * See xrBeginFrame.
	 *
	 * Must have made a call to either @ref predict_frame or @ref wait_frame
	 * before calling this function. After this function is called you must
	 * call layer_commit.
	 *
	 * @param[in] xc       The compositor
	 * @param[in] frame_id Frame id
	 */
	xrt_result_t (*begin_frame)(struct xrt_compositor *xc, int64_t frame_id);

	/*!
	 * @brief Explicitly discard a frame.
	 *
	 * This isn't in the OpenXR API but is explicit in the XRT interfaces.
	 *
	 * Two calls to xrBeginFrame without intervening xrEndFrame will cause
	 * the state tracker to call:
	 *
	 * ```c
	 * // first xrBeginFrame
	 * xrt_comp_begin_frame(xc, frame_id);
	 * // second xrBeginFrame
	 * xrt_comp_discard_frame(xc, frame_id);
	 * xrt_comp_begin_frame(xc, frame_id);
	 * ```
	 */
	xrt_result_t (*discard_frame)(struct xrt_compositor *xc, int64_t frame_id);

	/*! @} */


	/*!
	 * @name Function pointers for layer submission
	 * @{
	 */
	/*!
	 * @brief Begins layer submission.
	 *
	 * This and the other `layer_*` calls are equivalent to xrEndFrame,
	 * except split over multiple calls. It's only after
	 * xrt_compositor::layer_commit that layers will be displayed.
	 * From the point of view of the swapchain, the image is used as
	 * soon as it's given in a call.
	 */
	xrt_result_t (*layer_begin)(struct xrt_compositor *xc, const struct xrt_layer_frame_data *data);

	/*!
	 * @brief Adds a projection layer for submissions.
	 *
	 * Note that e.g. the same swapchain object may be passed as both
	 * @p l_xsc and @p r_xsc - the parameters in @p data identify
	 * the subrect and array texture index to use for each of the views.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param xsc         Swapchain object containing eye RGB data.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what parts of the supplied swapchain
	 *                    objects to use for each view.
	 */
	xrt_result_t (*layer_projection)(struct xrt_compositor *xc,
	                                 struct xrt_device *xdev,
	                                 struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
	                                 const struct xrt_layer_data *data);

	/*!
	 * @brief Adds a projection layer for submission, has depth information.
	 *
	 * Note that e.g. the same swapchain object may be passed as both
	 * @p l_xsc and @p r_xsc - the parameters in @p data identify
	 * the subrect and array texture index to use for each of the views.
	 * This flexibility is required by the OpenXR API and is passed through
	 * to the compositor to preserve the maximum information
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param l_xsc       Swapchain object containing left eye RGB data.
	 * @param r_xsc       Swapchain object containing right eye RGB data.
	 * @param l_d_xsc     Swapchain object containing left eye depth data.
	 * @param r_d_xsc     Swapchain object containing right eye depth data.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what parts of the supplied swapchain
	 *                    objects to use for each view.
	 */
	xrt_result_t (*layer_projection_depth)(struct xrt_compositor *xc,
	                                       struct xrt_device *xdev,
	                                       struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
	                                       struct xrt_swapchain *d_xsc[XRT_MAX_VIEWS],
	                                       const struct xrt_layer_data *data);

	/*!
	 * Adds a quad layer for submission, the center of the quad is specified
	 * by the pose and extends outwards from it.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param xsc         Swapchain.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what part of the supplied swapchain
	 *                    object to use.
	 */
	xrt_result_t (*layer_quad)(struct xrt_compositor *xc,
	                           struct xrt_device *xdev,
	                           struct xrt_swapchain *xsc,
	                           const struct xrt_layer_data *data);

	/*!
	 * Adds a cube layer for submission.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param xsc         Swapchain.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what part of the supplied swapchain
	 *                    object to use.
	 */
	xrt_result_t (*layer_cube)(struct xrt_compositor *xc,
	                           struct xrt_device *xdev,
	                           struct xrt_swapchain *xsc,
	                           const struct xrt_layer_data *data);

	/*!
	 * Adds a cylinder layer for submission.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param xsc         Swapchain.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what part of the supplied swapchain
	 *                    object to use.
	 */
	xrt_result_t (*layer_cylinder)(struct xrt_compositor *xc,
	                               struct xrt_device *xdev,
	                               struct xrt_swapchain *xsc,
	                               const struct xrt_layer_data *data);

	/*!
	 * Adds a equirect1 layer for submission.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param xsc         Swapchain.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what part of the supplied swapchain
	 *                    object to use.
	 */
	xrt_result_t (*layer_equirect1)(struct xrt_compositor *xc,
	                                struct xrt_device *xdev,
	                                struct xrt_swapchain *xsc,
	                                const struct xrt_layer_data *data);


	/*!
	 * Adds a equirect2 layer for submission.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param xsc         Swapchain.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what part of the supplied swapchain
	 *                    object to use.
	 */
	xrt_result_t (*layer_equirect2)(struct xrt_compositor *xc,
	                                struct xrt_device *xdev,
	                                struct xrt_swapchain *xsc,
	                                const struct xrt_layer_data *data);

	/*!
	 * Adds a passthrough layer for submission.
	 *
	 * @param xc          Self pointer
	 * @param xdev        The device the layer is relative to.
	 * @param data        All of the pure data bits (not pointers/handles),
	 *                    including what part of the supplied swapchain
	 *                    object to use.
	 */
	xrt_result_t (*layer_passthrough)(struct xrt_compositor *xc,
	                                  struct xrt_device *xdev,
	                                  const struct xrt_layer_data *data);

	/*!
	 * @brief Commits all of the submitted layers.
	 *
	 * Only after this call will the compositor actually use the layers.
	 */
	xrt_result_t (*layer_commit)(struct xrt_compositor *xc, xrt_graphics_sync_handle_t sync_handle);

	/*!
	 * @brief Commits all of the submitted layers, with a semaphore.
	 *
	 * Only after this call will the compositor actually use the layers.
	 * @param xc          Self pointer
	 * @param frame_id    The frame id this commit is for.
	 * @param xcsem       Semaphore that will be signalled when the app GPU
	 *                    work has completed.
	 * @param value       Semaphore value upone completion of GPU work.
	 */
	xrt_result_t (*layer_commit_with_semaphore)(struct xrt_compositor *xc,
	                                            struct xrt_compositor_semaphore *xcsem,
	                                            uint64_t value);

	/*! @} */


	/*!
	 * @name Function pointers for XR_FB_display_refresh_rate.
	 * @{
	 */

	/*!
	 * Get the current display refresh rate.
	 *
	 * @param xc          				    Self pointer
	 * @param out_display_refresh_rate_hz   Current display refresh rate in Hertz.
	 */
	xrt_result_t (*get_display_refresh_rate)(struct xrt_compositor *xc, float *out_display_refresh_rate_hz);

	/*!
	 * Request system to change the display refresh rate to the requested value.
	 *
	 * @param xc          			     Self pointer
	 * @param display_refresh_rate_hz    Requested display refresh rate in Hertz.
	 */
	xrt_result_t (*request_display_refresh_rate)(struct xrt_compositor *xc, float display_refresh_rate_hz);

	/*! @} */


	/*!
	 * @brief Set CPU/GPU performance level.
	 */
	xrt_result_t (*set_performance_level)(struct xrt_compositor *xc,
	                                      enum xrt_perf_domain domain,
	                                      enum xrt_perf_set_level level);

	/*!
	 * @brief Get the extents of the reference space’s bounds rectangle.
	 */
	xrt_result_t (*get_reference_bounds_rect)(struct xrt_compositor *xc,
	                                          enum xrt_reference_space_type reference_space_type,
	                                          struct xrt_vec2 *bounds);

	/*!
	 * Teardown the compositor.
	 *
	 * The state tracker must have made sure that no frames or sessions are
	 * currently pending.
	 *
	 * @see xrt_compositor::discard_frame or xrt_compositor::end_frame for a pending frame
	 * @see xrt_compositor::end_session for an open session.
	 */
	void (*destroy)(struct xrt_compositor *xc);

	/*!
	 * @name Function pointers for extensions
	 * @{
	 */

	/*!
	 * @brief Set thread attributes according to thread type
	 */
	xrt_result_t (*set_thread_hint)(struct xrt_compositor *xc, enum xrt_thread_hint hint, uint32_t thread_id);

	/*! @} */
};

/*!
 * @copydoc xrt_compositor::get_swapchain_create_properties
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_get_swapchain_create_properties(struct xrt_compositor *xc,
                                         const struct xrt_swapchain_create_info *info,
                                         struct xrt_swapchain_create_properties *xsccp)
{
	return xc->get_swapchain_create_properties(xc, info, xsccp);
}

/*!
 * @name Swapchain and sync creation and import methods
 * @{
 */

/*!
 * @copydoc xrt_compositor::create_swapchain
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_create_swapchain(struct xrt_compositor *xc,
                          const struct xrt_swapchain_create_info *info,
                          struct xrt_swapchain **out_xsc)
{
	return xc->create_swapchain(xc, info, out_xsc);
}

/*!
 * @copydoc xrt_compositor::import_swapchain
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_import_swapchain(struct xrt_compositor *xc,
                          const struct xrt_swapchain_create_info *info,
                          struct xrt_image_native *native_images,
                          uint32_t image_count,
                          struct xrt_swapchain **out_xsc)
{
	return xc->import_swapchain(xc, info, native_images, image_count, out_xsc);
}

/*!
 * @copydoc xrt_compositor::import_fence
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_import_fence(struct xrt_compositor *xc,
                      xrt_graphics_sync_handle_t handle,
                      struct xrt_compositor_fence **out_xcf)
{
	return xc->import_fence(xc, handle, out_xcf);
}

/*!
 * @copydoc xrt_compositor::create_semaphore
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_create_semaphore(struct xrt_compositor *xc,
                          xrt_graphics_sync_handle_t *out_handle,
                          struct xrt_compositor_semaphore **out_xcsem)
{
	return xc->create_semaphore(xc, out_handle, out_xcsem);
}

/*! @} */

/*!
 * @copydoc xrt_compositor::create_passthrough
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_create_passthrough(struct xrt_compositor *xc, const struct xrt_passthrough_create_info *info)
{
	return xc->create_passthrough(xc, info);
}

/*!
 * @copydoc xrt_compositor::create_passthrough_layer
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_create_passthrough_layer(struct xrt_compositor *xc, const struct xrt_passthrough_layer_create_info *info)
{
	return xc->create_passthrough_layer(xc, info);
}

/*!
 * @copydoc xrt_compositor::destroy_passthrough
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_destroy_passthrough(struct xrt_compositor *xc)
{
	return xc->destroy_passthrough(xc);
}

/*!
 * @name Session methods
 * @{
 */

/*!
 * @copydoc xrt_compositor::begin_session
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_begin_session(struct xrt_compositor *xc, const struct xrt_begin_session_info *info)
{
	return xc->begin_session(xc, info);
}

/*!
 * @copydoc xrt_compositor::end_session
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_end_session(struct xrt_compositor *xc)
{
	return xc->end_session(xc);
}

/*! @} */


/*!
 * @name Frame-related methods
 * @brief Related to the OpenXR `xr*Frame` functions
 * @{
 */

/*!
 * @copydoc xrt_compositor::predict_frame
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_predict_frame(struct xrt_compositor *xc,
                       int64_t *out_frame_id,
                       uint64_t *out_wake_time_ns,
                       uint64_t *out_predicted_gpu_time_ns,
                       uint64_t *out_predicted_display_time_ns,
                       uint64_t *out_predicted_display_period_ns)
{
	return xc->predict_frame(             //
	    xc,                               //
	    out_frame_id,                     //
	    out_wake_time_ns,                 //
	    out_predicted_gpu_time_ns,        //
	    out_predicted_display_time_ns,    //
	    out_predicted_display_period_ns); //
}

/*!
 * @copydoc xrt_compositor::mark_frame
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_mark_frame(struct xrt_compositor *xc,
                    int64_t frame_id,
                    enum xrt_compositor_frame_point point,
                    uint64_t when_ns)
{
	return xc->mark_frame(xc, frame_id, point, when_ns);
}

/*!
 * @copydoc xrt_compositor::wait_frame
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_wait_frame(struct xrt_compositor *xc,
                    int64_t *out_frame_id,
                    uint64_t *out_predicted_display_time,
                    uint64_t *out_predicted_display_period)
{
	return xc->wait_frame(xc, out_frame_id, out_predicted_display_time, out_predicted_display_period);
}

/*!
 * @copydoc xrt_compositor::begin_frame
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_begin_frame(struct xrt_compositor *xc, int64_t frame_id)
{
	return xc->begin_frame(xc, frame_id);
}

/*!
 * @copydoc xrt_compositor::discard_frame
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_discard_frame(struct xrt_compositor *xc, int64_t frame_id)
{
	return xc->discard_frame(xc, frame_id);
}

/*! @} */


/*!
 * @name Layer submission methods
 * @brief Equivalent to `xrEndFrame`, but split across multiple calls.
 * @{
 */

/*!
 * @copydoc xrt_compositor::layer_begin
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_begin(struct xrt_compositor *xc, const struct xrt_layer_frame_data *data)
{
	return xc->layer_begin(xc, data);
}

/*!
 * @copydoc xrt_compositor::layer_projection
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_projection(struct xrt_compositor *xc,
                          struct xrt_device *xdev,
                          struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
                          const struct xrt_layer_data *data)
{
	return xc->layer_projection(xc, xdev, xsc, data);
}

/*!
 * @copydoc xrt_compositor::layer_projection_depth
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_projection_depth(struct xrt_compositor *xc,
                                struct xrt_device *xdev,
                                struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
                                struct xrt_swapchain *d_xsc[XRT_MAX_VIEWS],
                                const struct xrt_layer_data *data)
{
	return xc->layer_projection_depth(xc, xdev, xsc, d_xsc, data);
}

/*!
 * @copydoc xrt_compositor::layer_quad
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_quad(struct xrt_compositor *xc,
                    struct xrt_device *xdev,
                    struct xrt_swapchain *xsc,
                    const struct xrt_layer_data *data)
{
	return xc->layer_quad(xc, xdev, xsc, data);
}

/*!
 * @copydoc xrt_compositor::layer_cube
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_cube(struct xrt_compositor *xc,
                    struct xrt_device *xdev,
                    struct xrt_swapchain *xsc,
                    const struct xrt_layer_data *data)
{
	return xc->layer_cube(xc, xdev, xsc, data);
}

/*!
 * @copydoc xrt_compositor::layer_cylinder
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_cylinder(struct xrt_compositor *xc,
                        struct xrt_device *xdev,
                        struct xrt_swapchain *xsc,
                        const struct xrt_layer_data *data)
{
	return xc->layer_cylinder(xc, xdev, xsc, data);
}


/*!
 * @copydoc xrt_compositor::layer_equirect1
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_equirect1(struct xrt_compositor *xc,
                         struct xrt_device *xdev,
                         struct xrt_swapchain *xsc,
                         const struct xrt_layer_data *data)
{
	return xc->layer_equirect1(xc, xdev, xsc, data);
}

/*!
 * @copydoc xrt_compositor::layer_equirect2
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_equirect2(struct xrt_compositor *xc,
                         struct xrt_device *xdev,
                         struct xrt_swapchain *xsc,
                         const struct xrt_layer_data *data)
{
	return xc->layer_equirect2(xc, xdev, xsc, data);
}

/*!
 * @copydoc xrt_compositor::layer_passthrough
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_passthrough(struct xrt_compositor *xc, struct xrt_device *xdev, const struct xrt_layer_data *data)
{
	return xc->layer_passthrough(xc, xdev, data);
}

/*!
 * @copydoc xrt_compositor::layer_commit
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_commit(struct xrt_compositor *xc, xrt_graphics_sync_handle_t sync_handle)
{
	return xc->layer_commit(xc, sync_handle);
}

/*!
 * @copydoc xrt_compositor::layer_commit_with_semaphore
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_layer_commit_with_semaphore(struct xrt_compositor *xc, struct xrt_compositor_semaphore *xcsem, uint64_t value)
{
	return xc->layer_commit_with_semaphore(xc, xcsem, value);
}

/*! @} */

/*!
 * @copydoc xrt_compositor::get_display_refresh_rate
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_get_display_refresh_rate(struct xrt_compositor *xc, float *out_display_refresh_rate_hz)
{
	return xc->get_display_refresh_rate(xc, out_display_refresh_rate_hz);
}

/*!
 * @copydoc xrt_compositor::request_display_refresh_rate
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_request_display_refresh_rate(struct xrt_compositor *xc, float display_refresh_rate_hz)
{
	return xc->request_display_refresh_rate(xc, display_refresh_rate_hz);
}


/*!
 * @copydoc xrt_compositor::set_performance_level
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_set_performance_level(struct xrt_compositor *xc, enum xrt_perf_domain domain, enum xrt_perf_set_level level)
{
	return xc->set_performance_level(xc, domain, level);
}

/*!
 * @copydoc xrt_compositor::get_reference_bounds_rect
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_compositor
 */
static inline xrt_result_t
xrt_comp_get_reference_bounds_rect(struct xrt_compositor *xc,
                                   enum xrt_reference_space_type reference_space_type,
                                   struct xrt_vec2 *bounds)
{
	if (xc->get_reference_bounds_rect == NULL) {
		bounds->x = 0.f;
		bounds->y = 0.f;
		return XRT_ERROR_COMPOSITOR_FUNCTION_NOT_IMPLEMENTED;
	}

	return xc->get_reference_bounds_rect(xc, reference_space_type, bounds);
}

/*!
 * @copydoc xrt_compositor::destroy
 *
 * Helper for calling through the function pointer: does a null check and sets
 * xc_ptr to null if freed.
 *
 * @public @memberof xrt_compositor
 */
static inline void
xrt_comp_destroy(struct xrt_compositor **xc_ptr)
{
	struct xrt_compositor *xc = *xc_ptr;
	if (xc == NULL) {
		return;
	}

	xc->destroy(xc);
	*xc_ptr = NULL;
}

/*!
 * @name Function pointers for extensions
 * @{
 */

/*!
 * @brief Set thread attributes according to thread type
 */
static inline xrt_result_t
xrt_comp_set_thread_hint(struct xrt_compositor *xc, enum xrt_thread_hint hint, uint32_t thread_id)
{
	return xc->set_thread_hint(xc, hint, thread_id);
}

/*! @} */

/*
 *
 * OpenGL interface.
 *
 */

/*!
 * Base class for an OpenGL (ES) client swapchain.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_swapchain
 */
struct xrt_swapchain_gl
{
	//! @public Base
	struct xrt_swapchain base;

	// GLuint
	unsigned int images[XRT_MAX_SWAPCHAIN_IMAGES];
};

/*!
 * Base class for an OpenGL (ES) client compositor.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_compositor
 */
struct xrt_compositor_gl
{
	struct xrt_compositor base;
};

/*!
 * Down-cast helper.
 *
 * @private @memberof xrt_swapchain_gl
 *
 * @todo unused - remove?
 */
static inline struct xrt_swapchain_gl *
xrt_swapchain_gl(struct xrt_swapchain *xsc)
{
	return (struct xrt_swapchain_gl *)xsc;
}

/*!
 * Down-cast helper.
 *
 * @private @memberof xrt_compositor_gl
 *
 * @todo unused - remove?
 */
static inline struct xrt_compositor_gl *
xrt_compositor_gl(struct xrt_compositor *xc)
{
	return (struct xrt_compositor_gl *)xc;
}


/*
 *
 * Vulkan interface.
 *
 */

/*!
 * Base class for a Vulkan client swapchain.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_swapchain
 */
struct xrt_swapchain_vk
{
	//! @public Base
	struct xrt_swapchain base;

	//! Images to be used by the caller.
	VkImage images[XRT_MAX_SWAPCHAIN_IMAGES];
};

/*!
 * Base class for a Vulkan client compositor.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_compositor
 */
struct xrt_compositor_vk
{
	//! @public Base
	struct xrt_compositor base;
};

/*!
 * Down-cast helper.
 *
 * @private @memberof xrt_swapchain_vk
 *
 * @todo unused - remove?
 */
static inline struct xrt_swapchain_vk *
xrt_swapchain_vk(struct xrt_swapchain *xsc)
{
	return (struct xrt_swapchain_vk *)xsc;
}

/*!
 * Down-cast helper.
 *
 * @private @memberof xrt_compositor_vk
 *
 * @todo unused - remove?
 */
static inline struct xrt_compositor_vk *
xrt_compositor_vk(struct xrt_compositor *xc)
{
	return (struct xrt_compositor_vk *)xc;
}

#if defined(XRT_HAVE_D3D11) || defined(XRT_DOXYGEN)

/*
 *
 * D3D11 interface.
 *
 */

/*!
 * Base class for a D3D11 client swapchain.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_swapchain
 */
struct xrt_swapchain_d3d11
{
	//! @public Base
	struct xrt_swapchain base;

	//! Images to be used by the caller.
	ID3D11Texture2D *images[XRT_MAX_SWAPCHAIN_IMAGES];
};

/*!
 * Base class for a D3D11 client compositor.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_compositor
 */
struct xrt_compositor_d3d11
{
	//! @public Base
	struct xrt_compositor base;
};

/*!
 * Graphics usage requirements for D3D APIs.
 *
 * @ingroup xrt_iface
 */
struct xrt_d3d_requirements
{
	LUID adapter_luid;
	D3D_FEATURE_LEVEL min_feature_level;
};

#endif // XRT_OS_WINDOWS


#if defined(XRT_HAVE_D3D12) || defined(XRT_DOXYGEN)
/*
 *
 * D3D12 interface.
 *
 */

/*!
 * Base class for a D3D12 client swapchain.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_swapchain
 */
struct xrt_swapchain_d3d12
{
	//! @public Base
	struct xrt_swapchain base;

	//! Images to be used by the caller.
	ID3D12Resource *images[XRT_MAX_SWAPCHAIN_IMAGES];
};

/*!
 * Base class for a D3D12 client compositor.
 *
 * @ingroup xrt_iface comp_client
 * @extends xrt_compositor
 */
struct xrt_compositor_d3d12
{
	//! @public Base
	struct xrt_compositor base;
};
#endif

/*
 *
 * Native interface.
 *
 * These types are supported by underlying native buffers, which are DMABUF file
 * descriptors on Linux.
 *
 */

/*!
 * A single image of a swapchain based on native buffer handles.
 *
 * @ingroup xrt_iface comp
 * @see xrt_swapchain_native, xrt_graphics_buffer_handle_t
 */
struct xrt_image_native
{
	/*!
	 * Native buffer handle.
	 */
	xrt_graphics_buffer_handle_t handle;

	/*!
	 * @brief Buffer size in memory.
	 *
	 * Optional, set to 0 if unknown at allocation time.
	 * If not zero, used for a max memory requirements check when importing
	 * into Vulkan.
	 */
	size_t size;

	/*!
	 * Is the image created with a dedicated allocation or not.
	 */
	bool use_dedicated_allocation;

	/*!
	 * Is the native buffer handle a DXGI handle?
	 */
	bool is_dxgi_handle;
};

/*!
 * @interface xrt_swapchain_native
 * Base class for a swapchain that exposes a native buffer handle to be imported
 * into a client API.
 *
 * @ingroup xrt_iface comp
 * @extends xrt_swapchain
 */
struct xrt_swapchain_native
{
	//! @public Base
	struct xrt_swapchain base;

	/*!
	 * Unique id for the swapchain, only unique for the current process, is
	 * not synchronized between service and any apps via the IPC layer.
	 */
	xrt_limited_unique_id_t limited_unique_id;

	struct xrt_image_native images[XRT_MAX_SWAPCHAIN_IMAGES];
};

/*!
 * @copydoc xrt_swapchain_reference
 *
 * @relates xrt_swapchain_native
 */
static inline void
xrt_swapchain_native_reference(struct xrt_swapchain_native **dst, struct xrt_swapchain_native *src)
{
	xrt_swapchain_reference((struct xrt_swapchain **)dst, (struct xrt_swapchain *)src);
}

/*!
 * @interface xrt_compositor_native
 *
 * Main compositor server interface.
 *
 * @ingroup xrt_iface comp
 * @extends xrt_compositor
 */
struct xrt_compositor_native
{
	//! @public Base
	struct xrt_compositor base;
};

/*!
 * @brief Create a native swapchain with a set of images.
 *
 * A specialized version of @ref xrt_comp_create_swapchain, for use only on @ref
 * xrt_compositor_native.
 *
 * Helper for calling through the base's function pointer then performing the
 * known-safe downcast.
 *
 * The pointer pointed to by @p out_xsc has to either be NULL or a valid
 * @ref xrt_swapchain pointer. If there is a valid @ref xrt_swapchain
 * pointed by the pointed pointer it will have it reference decremented.
 *
 * @public @memberof xrt_compositor_native
 */
static inline xrt_result_t
xrt_comp_native_create_swapchain(struct xrt_compositor_native *xcn,
                                 const struct xrt_swapchain_create_info *info,
                                 struct xrt_swapchain_native **out_xscn)
{
	struct xrt_swapchain *xsc = NULL; // Has to be NULL.

	xrt_result_t ret = xrt_comp_create_swapchain(&xcn->base, info, &xsc);
	if (ret == XRT_SUCCESS) {
		// Need to unref any swapchain already there first.
		xrt_swapchain_native_reference(out_xscn, NULL);

		// Already referenced.
		*out_xscn = (struct xrt_swapchain_native *)xsc;
	}

	return ret;
}

/*!
 * @copydoc xrt_compositor::destroy
 *
 * Helper for calling through the function pointer: does a null check and sets
 * xcn_ptr to null if freed.
 *
 * @public @memberof xrt_compositor_native
 */
static inline void
xrt_comp_native_destroy(struct xrt_compositor_native **xcn_ptr)
{
	struct xrt_compositor_native *xcn = *xcn_ptr;
	if (xcn == NULL) {
		return;
	}

	xcn->base.destroy(&xcn->base);
	*xcn_ptr = NULL;
}


/*
 *
 * System composition: how to composite on a system, either directly or by combining layers from multiple apps
 *
 */

/*!
 * Capabilities and information about the system compositor (and its wrapped native compositor, if any),
 * and device together.
 */
struct xrt_system_compositor_info
{
	struct
	{
		struct
		{
			uint32_t width_pixels;
			uint32_t height_pixels;
			uint32_t sample_count;
		} recommended; //!< Recommended for this view.

		struct
		{
			uint32_t width_pixels;
			uint32_t height_pixels;
			uint32_t sample_count;
		} max;          //!< Maximums for this view.
	} views[XRT_MAX_VIEWS]; //!< View configuration information.

	//! Maximum number of composition layers supported, never changes.
	uint32_t max_layers;

	/*!
	 * Blend modes supported by the system (the combination of the
	 * compositor and the HMD capabilities), never changes.
	 *
	 * In preference order. Based on the modes reported by the device,
	 * but the compositor has a chance to modify this.
	 */
	enum xrt_blend_mode supported_blend_modes[XRT_BLEND_MODE_MAX_ENUM];

	//! Number of meaningful elements in xrt_system_compositor_info::supported_blend_modes
	uint8_t supported_blend_mode_count;

	uint32_t refresh_rate_count;
	float refresh_rates_hz[XRT_MAX_SUPPORTED_REFRESH_RATES];

	//! The vk device as used by the compositor, never changes.
	xrt_uuid_t compositor_vk_deviceUUID;

	//! The vk device suggested for Vulkan clients, never changes.
	xrt_uuid_t client_vk_deviceUUID;

	//! The (Windows) LUID for the GPU device suggested for D3D clients, never changes.
	xrt_luid_t client_d3d_deviceLUID;

	//! Whether @ref client_d3d_deviceLUID is valid
	bool client_d3d_deviceLUID_valid;
};

struct xrt_system_compositor;

/*!
 * @interface xrt_multi_compositor_control
 * Special functions to control multi session/clients.
 * Effectively an optional aspect of @ref xrt_system_compositor
 * exposed by implementations that can combine layers from multiple sessions/clients.
 */
struct xrt_multi_compositor_control
{
	/*!
	 * Sets the state of the compositor, generating any events to the client
	 * if the state is actually changed. Input focus is enforced/handled by
	 * a different component but is still signaled by the compositor.
	 */
	xrt_result_t (*set_state)(struct xrt_system_compositor *xsc,
	                          struct xrt_compositor *xc,
	                          bool visible,
	                          bool focused);

	/*!
	 * Set the rendering Z order for rendering, visible has higher priority
	 * then z_order but is still saved until visible again. This a signed
	 * 64 bit integer compared to a unsigned 32 bit integer in OpenXR, so
	 * that non-overlay clients can be handled like overlay ones.
	 */
	xrt_result_t (*set_z_order)(struct xrt_system_compositor *xsc, struct xrt_compositor *xc, int64_t z_order);

	/*!
	 * Tell this client/session if the main application is visible or not.
	 */
	xrt_result_t (*set_main_app_visibility)(struct xrt_system_compositor *xsc,
	                                        struct xrt_compositor *xc,
	                                        bool visible);

	/*!
	 * Notify this client/session if the compositor is going to lose the ability of rendering.
	 *
	 * @param loss_time_ns System monotonic timestamps, such as returned by os_monotonic_get_ns().
	 */
	xrt_result_t (*notify_loss_pending)(struct xrt_system_compositor *xsc,
	                                    struct xrt_compositor *xc,
	                                    uint64_t loss_time_ns);

	/*!
	 * Notify this client/session if the compositor lost the ability of rendering.
	 */
	xrt_result_t (*notify_lost)(struct xrt_system_compositor *xsc, struct xrt_compositor *xc);

	/*!
	 * Notify this client/session if the display refresh rate has been changed.
	 */
	xrt_result_t (*notify_display_refresh_changed)(struct xrt_system_compositor *xsc,
	                                               struct xrt_compositor *xc,
	                                               float from_display_refresh_rate_hz,
	                                               float to_display_refresh_rate_hz);
};

/*!
 * The system compositor handles composition for a system.
 * It is not itself a "compositor" (as in xrt_compositor), but it can create/own compositors.
 * - In a multi-app capable system, the system compositor may own an internal compositor, and
 *   xrt_system_compositor::create_native_compositor will
 *   create a compositor that submits layers to a merging mechanism.
 * - In a non-multi-app capable system, xrt_system_compositor::create_native_compositor
 *   creates normal, native compositors, that do not wrap or feed into any other compositor.
 *
 * This is a long lived object: it has the same life time as an XrSystemID.
 */
struct xrt_system_compositor
{
	/*!
	 * An optional aspect/additional interface, providing multi-app control.
	 * Populated if this system compositor supports multi client controls.
	 */
	struct xrt_multi_compositor_control *xmcc;

	//! Info regarding the system.
	struct xrt_system_compositor_info info;

	/*!
	 * Create a new native compositor.
	 *
	 * This signals that you want to start XR, and as such implicitly brings
	 * up a new session. Does not "call" `xrBeginSession`.
	 *
	 * Some system compositors might only support one `xrt_compositor`
	 * active at a time, will return `XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED`
	 * if this is the case.
	 *
	 * In a multi-session capable system compositor, this may return a "proxy"
	 * for feeding a single client's layers to a compositor or a layer merging mechanism,
	 * rather than a raw native compositor (not wrapping or forwarding) directly.
	 */
	xrt_result_t (*create_native_compositor)(struct xrt_system_compositor *xsc,
	                                         const struct xrt_session_info *xsi,
	                                         struct xrt_session_event_sink *xses,
	                                         struct xrt_compositor_native **out_xcn);

	/*!
	 * Teardown the system compositor.
	 *
	 * The state tracker must make sure that no compositors are alive.
	 */
	void (*destroy)(struct xrt_system_compositor *xsc);
};

/*!
 * @copydoc xrt_multi_compositor_control::set_state
 *
 * Helper for calling through the function pointer.
 *
 * If the system compositor @p xsc does not implement @ref xrt_multi_compositor_control,
 * this returns @ref XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_set_state(struct xrt_system_compositor *xsc, struct xrt_compositor *xc, bool visible, bool focused)
{
	if (xsc->xmcc == NULL) {
		return XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED;
	}

	return xsc->xmcc->set_state(xsc, xc, visible, focused);
}

/*!
 * @copydoc xrt_multi_compositor_control::set_z_order
 *
 * Helper for calling through the function pointer.
 *
 * If the system compositor @p xsc does not implement @ref xrt_multi_compositor_control,
 * this returns @ref XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_set_z_order(struct xrt_system_compositor *xsc, struct xrt_compositor *xc, int64_t z_order)
{
	if (xsc->xmcc == NULL) {
		return XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED;
	}

	return xsc->xmcc->set_z_order(xsc, xc, z_order);
}


/*!
 * @copydoc xrt_multi_compositor_control::set_main_app_visibility
 *
 * Helper for calling through the function pointer.
 *
 * If the system compositor @p xsc does not implement @ref xrt_multi_compositor_control,
 * this returns @ref XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_set_main_app_visibility(struct xrt_system_compositor *xsc, struct xrt_compositor *xc, bool visible)
{
	if (xsc->xmcc == NULL) {
		return XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED;
	}

	return xsc->xmcc->set_main_app_visibility(xsc, xc, visible);
}

/*!
 * @copydoc xrt_multi_compositor_control::notify_loss_pending
 *
 * Helper for calling through the function pointer.
 *
 * If the system compositor @p xsc does not implement @ref xrt_multi_compositor_control,
 * this returns @ref XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_notify_loss_pending(struct xrt_system_compositor *xsc, struct xrt_compositor *xc, uint64_t loss_time_ns)
{
	if (xsc->xmcc == NULL) {
		return XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED;
	}

	return xsc->xmcc->notify_loss_pending(xsc, xc, loss_time_ns);
}

/*!
 * @copydoc xrt_multi_compositor_control::notify_lost
 *
 * Helper for calling through the function pointer.
 *
 * If the system compositor @p xsc does not implement @ref xrt_multi_compositor_control,
 * this returns @ref XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_notify_lost(struct xrt_system_compositor *xsc, struct xrt_compositor *xc)
{
	if (xsc->xmcc == NULL) {
		return XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED;
	}

	return xsc->xmcc->notify_lost(xsc, xc);
}

/*!
 * @copydoc xrt_multi_compositor_control::notify_display_refresh_changed
 *
 * Helper for calling through the function pointer.
 *
 * If the system compositor @p xsc does not implement @ref xrt_multi_composition_control,
 * this returns @ref XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_notify_display_refresh_changed(struct xrt_system_compositor *xsc,
                                           struct xrt_compositor *xc,
                                           float from_display_refresh_rate_hz,
                                           float to_display_refresh_rate_hz)
{
	if (xsc->xmcc == NULL) {
		return XRT_ERROR_MULTI_SESSION_NOT_IMPLEMENTED;
	}

	return xsc->xmcc->notify_display_refresh_changed(xsc, xc, from_display_refresh_rate_hz,
	                                                 to_display_refresh_rate_hz);
}

/*!
 * @copydoc xrt_system_compositor::create_native_compositor
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_system_compositor
 */
static inline xrt_result_t
xrt_syscomp_create_native_compositor(struct xrt_system_compositor *xsc,
                                     const struct xrt_session_info *xsi,
                                     struct xrt_session_event_sink *xses,
                                     struct xrt_compositor_native **out_xcn)
{
	return xsc->create_native_compositor(xsc, xsi, xses, out_xcn);
}

/*!
 * @copydoc xrt_system_compositor::destroy
 *
 * Helper for calling through the function pointer: does a null check and sets
 * xcn_ptr to null if freed.
 *
 * @public @memberof xrt_system_compositor
 */
static inline void
xrt_syscomp_destroy(struct xrt_system_compositor **xsc_ptr)
{
	struct xrt_system_compositor *xsc = *xsc_ptr;
	if (xsc == NULL) {
		return;
	}

	xsc->destroy(xsc);
	*xsc_ptr = NULL;
}


/*
 *
 * Image allocator.
 *
 */

/*!
 * Allocator for system native images, in general you do not need to free the
 * images as they will be consumed by importing them to the graphics API.
 *
 * @see xrt_image_native
 */
struct xrt_image_native_allocator
{
	/*!
	 * Allocate a set of images suitable to be used to back a swapchain
	 * with the given create info properties (@p xsci).
	 */
	xrt_result_t (*images_allocate)(struct xrt_image_native_allocator *xina,
	                                const struct xrt_swapchain_create_info *xsci,
	                                size_t image_count,
	                                struct xrt_image_native *out_images);

	/*!
	 * Free the given images.
	 */
	xrt_result_t (*images_free)(struct xrt_image_native_allocator *xina,
	                            size_t image_count,
	                            struct xrt_image_native *images);

	/*!
	 * Destroy the image allocator.
	 */
	void (*destroy)(struct xrt_image_native_allocator *xina);
};

/*!
 * @copydoc xrt_image_native_allocator::xrt_images_allocate
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_image_native_allocate
 */
static inline xrt_result_t
xrt_images_allocate(struct xrt_image_native_allocator *xina,
                    const struct xrt_swapchain_create_info *xsci,
                    size_t image_count,
                    struct xrt_image_native *out_images)
{
	return xina->images_allocate(xina, xsci, image_count, out_images);
}

/*!
 * @copydoc xrt_image_native_allocator::images_free
 *
 * Helper for calling through the function pointer.
 *
 * @public @memberof xrt_image_native_allocate
 */
static inline xrt_result_t
xrt_images_free(struct xrt_image_native_allocator *xina, size_t image_count, struct xrt_image_native *images)
{
	return xina->images_free(xina, image_count, images);
}

/*!
 * @copydoc xrt_image_native_allocator::destroy
 *
 * Helper for calling through the function pointer: does a null check and sets
 * xina_ptr to null if freed.
 *
 * @public @memberof xrt_image_native_allocator
 */
static inline void
xrt_images_destroy(struct xrt_image_native_allocator **xina_ptr)
{
	struct xrt_image_native_allocator *xina = *xina_ptr;
	if (xina == NULL) {
		return;
	}

	xina->destroy(xina);
	*xina_ptr = NULL;
}


/*!
 * @}
 */


#ifdef __cplusplus
}
#endif

// Copyright 2019-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  D3D11 client side glue to compositor implementation.
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @author Fernando Velazquez Innella <finnella@magicleap.com>
 * @ingroup comp_client
 */

#include "comp_d3d11_client.h"

#include "comp_d3d_common.hpp"

#include "xrt/xrt_compositor.h"
#include "xrt/xrt_config_os.h"
#include "xrt/xrt_handles.h"
#include "xrt/xrt_deleters.hpp"
#include "xrt/xrt_results.h"
#include "xrt/xrt_vulkan_includes.h"
#include "d3d/d3d_dxgi_formats.h"
#include "d3d/d3d_d3d11_helpers.hpp"
#include "d3d/d3d_d3d11_allocator.hpp"
#include "d3d/d3d_d3d11_fence.hpp"
#include "util/u_misc.h"
#include "util/u_pretty_print.h"
#include "util/u_time.h"
#include "util/u_logging.h"
#include "util/u_debug.h"
#include "util/u_handles.h"
#include "util/u_win32_com_guard.hpp"

#include <d3d11_1.h>
#include <d3d11_3.h>
#include <wil/resource.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include <assert.h>
#include <inttypes.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>

using namespace std::chrono_literals;
using namespace std::chrono;

using xrt::compositor::client::unique_swapchain_ref;

DEBUG_GET_ONCE_LOG_OPTION(log, "D3D_COMPOSITOR_LOG", U_LOGGING_INFO)

/*!
 * Spew level logging.
 *
 * @relates client_d3d11_compositor
 */
#define D3D_SPEW(c, ...) U_LOG_IFL_T(c->log_level, __VA_ARGS__);

/*!
 * Debug level logging.
 *
 * @relates client_d3d11_compositor
 */
#define D3D_DEBUG(c, ...) U_LOG_IFL_D(c->log_level, __VA_ARGS__);

/*!
 * Info level logging.
 *
 * @relates client_d3d11_compositor
 */
#define D3D_INFO(c, ...) U_LOG_IFL_I(c->log_level, __VA_ARGS__);

/*!
 * Warn level logging.
 *
 * @relates client_d3d11_compositor
 */
#define D3D_WARN(c, ...) U_LOG_IFL_W(c->log_level, __VA_ARGS__);

/*!
 * Error level logging.
 *
 * @relates client_d3d11_compositor
 */
#define D3D_ERROR(c, ...) U_LOG_IFL_E(c->log_level, __VA_ARGS__);

using unique_compositor_semaphore_ref = std::unique_ptr<
    struct xrt_compositor_semaphore,
    xrt::deleters::reference_deleter<struct xrt_compositor_semaphore, xrt_compositor_semaphore_reference>>;

// 0 is special
static constexpr uint64_t kKeyedMutexKey = 0;

// Timeout to wait for completion
static constexpr auto kFenceTimeout = 500ms;

/*!
 * @class client_d3d11_compositor
 *
 * Wraps the real compositor providing a D3D11 based interface.
 *
 * @ingroup comp_client
 * @implements xrt_compositor_d3d11
 */
struct client_d3d11_compositor
{
	struct xrt_compositor_d3d11 base = {};

	//! Owning reference to the backing native compositor
	struct xrt_compositor_native *xcn{nullptr};

	//! Just keeps COM alive while we keep references to COM things.
	xrt::auxiliary::util::ComGuard com_guard;

	//! Logging level.
	enum u_logging_level log_level = U_LOGGING_INFO;

	//! Device we got from the app
	wil::com_ptr<ID3D11Device5> app_device;

	//! Immediate context for @ref app_device
	wil::com_ptr<ID3D11DeviceContext3> app_context;

	//! A similar device we created on the same adapter
	wil::com_ptr<ID3D11Device5> comp_device;

	//! Immediate context for @ref comp_device
	wil::com_ptr<ID3D11DeviceContext4> comp_context;

	//! Device used for the fence, currently the @ref app_device.
	wil::com_ptr<ID3D11Device5> fence_device;

	//! Immediate context for @ref fence_device
	wil::com_ptr<ID3D11DeviceContext4> fence_context;

	// wil::unique_handle timeline_semaphore_handle;

	/*!
	 * A timeline semaphore made by the native compositor and imported by us.
	 *
	 * When this is valid, we should use xrt_compositor::layer_commit_with_semaphone:
	 * it means the native compositor knows about timeline semaphores, and we can import its semaphores, so we can
	 * pass @ref timeline_semaphore instead of blocking locally.
	 */
	unique_compositor_semaphore_ref timeline_semaphore;

	/*!
	 * A fence (timeline semaphore) object, owned by @ref fence_device.
	 *
	 * Signal using @ref fence_context if this is not null.
	 *
	 * Wait on it in `layer_commit` if @ref timeline_semaphore *is* null/invalid.
	 */
	wil::com_ptr<ID3D11Fence> fence;

	/*!
	 * Event used for blocking in `layer_commit` if required (if @ref timeline_semaphore *is* null/invalid)
	 */
	wil::unique_event_nothrow local_wait_event;

	/*!
	 * The value most recently signaled on the timeline semaphore
	 */
	uint64_t timeline_semaphore_value = 0;
};

static_assert(std::is_standard_layout<client_d3d11_compositor>::value);

struct client_d3d11_swapchain;

static inline DWORD
convertTimeoutToWindowsMilliseconds(uint64_t timeout_ns)
{
	return (timeout_ns == XRT_INFINITE_DURATION) ? INFINITE : (DWORD)(timeout_ns / (uint64_t)U_TIME_1MS_IN_NS);
}

/*!
 * Split out from @ref client_d3d11_swapchain to ensure that it is standard
 * layout, std::vector for instance is not standard layout.
 */
struct client_d3d11_swapchain_data
{
	explicit client_d3d11_swapchain_data(enum u_logging_level log_level) : keyed_mutex_collection(log_level) {}

	xrt::compositor::client::KeyedMutexCollection keyed_mutex_collection;

	//! The shared DXGI handles for our images
	std::vector<HANDLE> dxgi_handles;

	//! Images associated with client_d3d11_compositor::app_device
	std::vector<wil::com_ptr<ID3D11Texture2D1>> app_images;

	//! Images associated with client_d3d11_compositor::comp_device
	std::vector<wil::com_ptr<ID3D11Texture2D1>> comp_images;
};

/*!
 * Wraps the real compositor swapchain providing a D3D11 based interface.
 *
 * @ingroup comp_client
 * @implements xrt_swapchain_d3d11
 */
struct client_d3d11_swapchain
{
	struct xrt_swapchain_d3d11 base;

	//! Owning reference to the imported swapchain.
	unique_swapchain_ref xsc;

	//! Non-owning reference to our parent compositor.
	struct client_d3d11_compositor *c;

	//! implementation struct with things that aren't standard_layout
	std::unique_ptr<client_d3d11_swapchain_data> data;
};

static_assert(std::is_standard_layout<client_d3d11_swapchain>::value);

/*!
 * Down-cast helper.
 * @private @memberof client_d3d11_swapchain
 */
static inline struct client_d3d11_swapchain *
as_client_d3d11_swapchain(struct xrt_swapchain *xsc)
{
	return reinterpret_cast<client_d3d11_swapchain *>(xsc);
}

/*!
 * Down-cast helper.
 * @private @memberof client_d3d11_compositor
 */
static inline struct client_d3d11_compositor *
as_client_d3d11_compositor(struct xrt_compositor *xc)
{
	return (struct client_d3d11_compositor *)xc;
}


/*
 *
 * Logging helper.
 *
 */
static constexpr size_t kErrorBufSize = 256;

template <size_t N>
static inline bool
formatMessage(DWORD err, char (&buf)[N])
{
	if (0 != FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
	                        LANG_SYSTEM_DEFAULT, buf, N - 1, NULL)) {
		return true;
	}
	memset(buf, 0, N);
	return false;
}


/*
 *
 * Swapchain functions.
 *
 */

static xrt_result_t
client_d3d11_swapchain_acquire_image(struct xrt_swapchain *xsc, uint32_t *out_index)
{
	struct client_d3d11_swapchain *sc = as_client_d3d11_swapchain(xsc);

	// Pipe down call into imported swapchain in native compositor.
	return xrt_swapchain_acquire_image(sc->xsc.get(), out_index);
}

static xrt_result_t
client_d3d11_swapchain_wait_image(struct xrt_swapchain *xsc, uint64_t timeout_ns, uint32_t index)
{
	struct client_d3d11_swapchain *sc = as_client_d3d11_swapchain(xsc);

	// Pipe down call into imported swapchain in native compositor.
	xrt_result_t xret = xrt_swapchain_wait_image(sc->xsc.get(), timeout_ns, index);

	if (xret == XRT_SUCCESS) {
		// OK, we got the image in the native compositor, now need the keyed mutex in d3d11.
		xret = sc->data->keyed_mutex_collection.waitKeyedMutex(index, timeout_ns);
	}

	//! @todo discard old contents?
	return xret;
}

static xrt_result_t
client_d3d11_swapchain_barrier_image(struct xrt_swapchain *xsc, enum xrt_barrier_direction direction, uint32_t index)
{
	return XRT_SUCCESS;
}

static xrt_result_t
client_d3d11_swapchain_release_image(struct xrt_swapchain *xsc, uint32_t index)
{
	struct client_d3d11_swapchain *sc = as_client_d3d11_swapchain(xsc);

	// Pipe down call into imported swapchain in native compositor.
	xrt_result_t xret = xrt_swapchain_release_image(sc->xsc.get(), index);

	if (xret == XRT_SUCCESS) {
		// Release the keyed mutex
		xret = sc->data->keyed_mutex_collection.releaseKeyedMutex(index);
	}
	return xret;
}

static void
client_d3d11_swapchain_destroy(struct xrt_swapchain *xsc)
{
	// letting destruction do it all
	std::unique_ptr<client_d3d11_swapchain> sc(as_client_d3d11_swapchain(xsc));
}

/*
 *
 * Import helpers
 *
 */

static wil::com_ptr<ID3D11Texture2D1>
import_image(ID3D11Device1 &device, HANDLE h)
{
	wil::com_ptr<ID3D11Texture2D1> tex;

	if (h == nullptr) {
		return {};
	}
	THROW_IF_FAILED(device.OpenSharedResource1(h, __uuidof(ID3D11Texture2D1), tex.put_void()));
	return tex;
}

static wil::com_ptr<ID3D11Texture2D1>
import_image_dxgi(ID3D11Device1 &device, HANDLE h)
{
	wil::com_ptr<ID3D11Texture2D1> tex;

	if (h == nullptr) {
		return {};
	}
	THROW_IF_FAILED(device.OpenSharedResource(h, __uuidof(ID3D11Texture2D1), tex.put_void()));
	return tex;
}

static wil::com_ptr<ID3D11Fence>
import_fence(ID3D11Device5 &device, HANDLE h)
{
	wil::com_ptr<ID3D11Fence> fence;

	if (h == nullptr) {
		return {};
	}
	THROW_IF_FAILED(device.OpenSharedFence(h, __uuidof(ID3D11Fence), fence.put_void()));
	return fence;
}


xrt_result_t
client_d3d11_create_swapchain(struct xrt_compositor *xc,
                              const struct xrt_swapchain_create_info *info,
                              struct xrt_swapchain **out_xsc)
try {
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);
	xrt_result_t xret = XRT_SUCCESS;
	xrt_swapchain_create_properties xsccp{};
	xret = xrt_comp_get_swapchain_create_properties(xc, info, &xsccp);

	if (xret != XRT_SUCCESS) {
		D3D_ERROR(c, "Could not get properties for creating swapchain");
		return xret;
	}
	uint32_t image_count = xsccp.image_count;


	if ((info->create & XRT_SWAPCHAIN_CREATE_PROTECTED_CONTENT) != 0) {
		D3D_WARN(c,
		         "Swapchain info is valid but this compositor doesn't support creating protected content "
		         "swapchains!");
		return XRT_ERROR_SWAPCHAIN_FLAG_VALID_BUT_UNSUPPORTED;
	}

	int64_t vk_format = d3d_dxgi_format_to_vk((DXGI_FORMAT)info->format);
	if (vk_format == 0) {
		D3D_ERROR(c, "Invalid format!");
		return XRT_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	struct xrt_swapchain_create_info xinfo = *info;
	struct xrt_swapchain_create_info vkinfo = *info;

	// Update the create info.
	xinfo.bits = (enum xrt_swapchain_usage_bits)(xsccp.extra_bits | xinfo.bits);
	vkinfo.format = vk_format;
	vkinfo.bits = (enum xrt_swapchain_usage_bits)(xsccp.extra_bits | vkinfo.bits);

	std::unique_ptr<struct client_d3d11_swapchain> sc = std::make_unique<struct client_d3d11_swapchain>();
	sc->data = std::make_unique<client_d3d11_swapchain_data>(c->log_level);
	auto &data = sc->data;
	xret = xrt::auxiliary::d3d::d3d11::allocateSharedImages(*(c->comp_device), xinfo, image_count, true,
	                                                        data->comp_images, data->dxgi_handles);
	if (xret != XRT_SUCCESS) {
		return xret;
	}

	data->app_images.reserve(image_count);

	// Import from the handle for the app.
	for (uint32_t i = 0; i < image_count; ++i) {
		wil::com_ptr<ID3D11Texture2D1> image = import_image_dxgi(*(c->app_device), data->dxgi_handles[i]);

		// Put the image where the OpenXR state tracker can get it
		sc->base.images[i] = image.get();

		// Store the owning pointer for lifetime management
		data->app_images.emplace_back(std::move(image));
	}

	// Cache the keyed mutex interface
	xret = data->keyed_mutex_collection.init(data->app_images);
	if (xret != XRT_SUCCESS) {
		D3D_ERROR(c, "Error retrieving keyex mutex interfaces");
		return xret;
	}

	// Import into the native compositor, to create the corresponding swapchain which we wrap.
	xret = xrt::compositor::client::importFromDxgiHandles(
	    *(c->xcn), data->dxgi_handles, vkinfo, false /** @todo not sure - dedicated allocation */, sc->xsc);

	if (xret != XRT_SUCCESS) {
		D3D_ERROR(c, "Error importing D3D11 swapchain into native compositor");
		return xret;
	}

	sc->base.base.destroy = client_d3d11_swapchain_destroy;
	sc->base.base.acquire_image = client_d3d11_swapchain_acquire_image;
	sc->base.base.wait_image = client_d3d11_swapchain_wait_image;
	sc->base.base.barrier_image = client_d3d11_swapchain_barrier_image;
	sc->base.base.release_image = client_d3d11_swapchain_release_image;
	sc->c = c;
	sc->base.base.image_count = image_count;

	xrt_swapchain_reference(out_xsc, &sc->base.base);
	(void)sc.release();
	return XRT_SUCCESS;
} catch (wil::ResultException const &e) {
	U_LOG_E("Error creating D3D11 swapchain: %s", e.what());
	return XRT_ERROR_ALLOCATION;
} catch (std::exception const &e) {
	U_LOG_E("Error creating D3D11 swapchain: %s", e.what());
	return XRT_ERROR_ALLOCATION;
} catch (...) {
	U_LOG_E("Error creating D3D11 swapchain");
	return XRT_ERROR_ALLOCATION;
}

static xrt_result_t
client_d3d11_compositor_passthrough_create(struct xrt_compositor *xc, const struct xrt_passthrough_create_info *info)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_create_passthrough(&c->xcn->base, info);
}

static xrt_result_t
client_d3d11_compositor_passthrough_layer_create(struct xrt_compositor *xc,
                                                 const struct xrt_passthrough_layer_create_info *info)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_create_passthrough_layer(&c->xcn->base, info);
}

static xrt_result_t
client_d3d11_compositor_passthrough_destroy(struct xrt_compositor *xc)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_destroy_passthrough(&c->xcn->base);
}

/*
 *
 * Compositor functions.
 *
 */


static xrt_result_t
client_d3d11_compositor_begin_session(struct xrt_compositor *xc, const struct xrt_begin_session_info *info)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_begin_session(&c->xcn->base, info);
}

static xrt_result_t
client_d3d11_compositor_end_session(struct xrt_compositor *xc)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_end_session(&c->xcn->base);
}

static xrt_result_t
client_d3d11_compositor_wait_frame(struct xrt_compositor *xc,
                                   int64_t *out_frame_id,
                                   uint64_t *predicted_display_time,
                                   uint64_t *predicted_display_period)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_wait_frame(&c->xcn->base, out_frame_id, predicted_display_time, predicted_display_period);
}

static xrt_result_t
client_d3d11_compositor_begin_frame(struct xrt_compositor *xc, int64_t frame_id)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_begin_frame(&c->xcn->base, frame_id);
}

static xrt_result_t
client_d3d11_compositor_discard_frame(struct xrt_compositor *xc, int64_t frame_id)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_discard_frame(&c->xcn->base, frame_id);
}

static xrt_result_t
client_d3d11_compositor_layer_begin(struct xrt_compositor *xc, const struct xrt_layer_frame_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// Pipe down call into native compositor.
	return xrt_comp_layer_begin(&c->xcn->base, data);
}

static xrt_result_t
client_d3d11_compositor_layer_projection(struct xrt_compositor *xc,
                                         struct xrt_device *xdev,
                                         struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
                                         const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_PROJECTION);

	struct xrt_swapchain *xscn[XRT_MAX_VIEWS];
	for (uint32_t i = 0; i < data->view_count; ++i) {
		xscn[i] = as_client_d3d11_swapchain(xsc[i])->xsc.get();
	}

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_projection(&c->xcn->base, xdev, xscn, data);
}

static xrt_result_t
client_d3d11_compositor_layer_projection_depth(struct xrt_compositor *xc,
                                               struct xrt_device *xdev,
                                               struct xrt_swapchain *xsc[XRT_MAX_VIEWS],
                                               struct xrt_swapchain *d_xsc[XRT_MAX_VIEWS],
                                               const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_PROJECTION_DEPTH);
	struct xrt_swapchain *xscn[XRT_MAX_VIEWS];
	struct xrt_swapchain *d_xscn[XRT_MAX_VIEWS];
	for (uint32_t i = 0; i < data->view_count; ++i) {
		xscn[i] = as_client_d3d11_swapchain(xsc[i])->xsc.get();
		d_xscn[i] = as_client_d3d11_swapchain(d_xsc[i])->xsc.get();
	}

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_projection_depth(&c->xcn->base, xdev, xscn, d_xscn, data);
}

static xrt_result_t
client_d3d11_compositor_layer_quad(struct xrt_compositor *xc,
                                   struct xrt_device *xdev,
                                   struct xrt_swapchain *xsc,
                                   const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_QUAD);

	struct xrt_swapchain *xscfb = as_client_d3d11_swapchain(xsc)->xsc.get();

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_quad(&c->xcn->base, xdev, xscfb, data);
}

static xrt_result_t
client_d3d11_compositor_layer_cube(struct xrt_compositor *xc,
                                   struct xrt_device *xdev,
                                   struct xrt_swapchain *xsc,
                                   const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_CUBE);

	struct xrt_swapchain *xscfb = as_client_d3d11_swapchain(xsc)->xsc.get();

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_cube(&c->xcn->base, xdev, xscfb, data);
}

static xrt_result_t
client_d3d11_compositor_layer_cylinder(struct xrt_compositor *xc,
                                       struct xrt_device *xdev,
                                       struct xrt_swapchain *xsc,
                                       const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_CYLINDER);

	struct xrt_swapchain *xscfb = as_client_d3d11_swapchain(xsc)->xsc.get();

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_cylinder(&c->xcn->base, xdev, xscfb, data);
}

static xrt_result_t
client_d3d11_compositor_layer_equirect1(struct xrt_compositor *xc,
                                        struct xrt_device *xdev,
                                        struct xrt_swapchain *xsc,
                                        const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_EQUIRECT1);

	struct xrt_swapchain *xscfb = as_client_d3d11_swapchain(xsc)->xsc.get();

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_equirect1(&c->xcn->base, xdev, xscfb, data);
}

static xrt_result_t
client_d3d11_compositor_layer_equirect2(struct xrt_compositor *xc,
                                        struct xrt_device *xdev,
                                        struct xrt_swapchain *xsc,
                                        const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_EQUIRECT2);

	struct xrt_swapchain *xscfb = as_client_d3d11_swapchain(xsc)->xsc.get();

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_equirect2(&c->xcn->base, xdev, xscfb, data);
}

static xrt_result_t
client_d3d11_compositor_layer_passthrough(struct xrt_compositor *xc,
                                          struct xrt_device *xdev,
                                          const struct xrt_layer_data *data)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	assert(data->type == XRT_LAYER_PASSTHROUGH);

	// No flip required: D3D11 swapchain image convention matches Vulkan.
	return xrt_comp_layer_passthrough(&c->xcn->base, xdev, data);
}

static xrt_result_t
client_d3d11_compositor_layer_commit(struct xrt_compositor *xc, xrt_graphics_sync_handle_t sync_handle)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	// We make the sync object, not st/oxr which is our user.
	assert(!xrt_graphics_sync_handle_is_valid(sync_handle));

	xrt_result_t xret = XRT_SUCCESS;
	if (c->fence) {
		c->timeline_semaphore_value++;
		HRESULT hr = c->fence_context->Signal(c->fence.get(), c->timeline_semaphore_value);
		if (!SUCCEEDED(hr)) {
			char buf[kErrorBufSize];
			formatMessage(hr, buf);
			D3D_ERROR(c, "Error signaling fence: %s", buf);
			return xrt_comp_layer_commit(&c->xcn->base, XRT_GRAPHICS_SYNC_HANDLE_INVALID);
		}
	}

	if (c->timeline_semaphore) {
		// We got this from the native compositor, so we can pass it back
		return xrt_comp_layer_commit_with_semaphore( //
		    &c->xcn->base,                           //
		    c->timeline_semaphore.get(),             //
		    c->timeline_semaphore_value);            //
	}

	if (c->fence) {
		// Wait on it ourselves, if we have it and didn't tell the native compositor to wait on it.
		xret = xrt::auxiliary::d3d::d3d11::waitOnFenceWithTimeout( //
		    c->fence,                                              //
		    c->local_wait_event,                                   //
		    c->timeline_semaphore_value,                           //
		    kFenceTimeout);                                        //
		if (xret != XRT_SUCCESS) {
			struct u_pp_sink_stack_only sink; // Not inited, very large.
			u_pp_delegate_t dg = u_pp_sink_stack_only_init(&sink);
			u_pp(dg, "Problem waiting on fence: ");
			u_pp_xrt_result(dg, xret);
			D3D_ERROR(c, "%s", sink.buffer);

			return xret;
		}
	}

	return xrt_comp_layer_commit(&c->xcn->base, XRT_GRAPHICS_SYNC_HANDLE_INVALID);
}


static xrt_result_t
client_d3d11_compositor_get_swapchain_create_properties(struct xrt_compositor *xc,
                                                        const struct xrt_swapchain_create_info *info,
                                                        struct xrt_swapchain_create_properties *xsccp)
{
	struct client_d3d11_compositor *c = as_client_d3d11_compositor(xc);

	int64_t vk_format = d3d_dxgi_format_to_vk((DXGI_FORMAT)info->format);
	if (vk_format == 0) {
		D3D_ERROR(c, "Invalid format!");
		return XRT_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	struct xrt_swapchain_create_info xinfo = *info;
	xinfo.format = vk_format;

	return xrt_comp_get_swapchain_create_properties(&c->xcn->base, &xinfo, xsccp);
}

static void
client_d3d11_compositor_destroy(struct xrt_compositor *xc)
{
	std::unique_ptr<struct client_d3d11_compositor> c{as_client_d3d11_compositor(xc)};
}

static void
client_d3d11_compositor_init_try_timeline_semaphores(struct client_d3d11_compositor *c)
{
	struct xrt_compositor_semaphore *xcsem{nullptr};
	HANDLE timeline_semaphore_handle_raw{};
	xrt_result_t xret;

	// Set the value to something non-zero.
	c->timeline_semaphore_value = 1;

	// See if we can make a "timeline semaphore", also known as ID3D11Fence
	if (!c->xcn->base.create_semaphore || !c->xcn->base.layer_commit_with_semaphore) {
		return;
	}

	/*
	 * This call returns a HANDLE in the out_handle argument, it is owned by
	 * the returned xrt_compositor_semaphore object we should not track it.
	 */
	xret = xrt_comp_create_semaphore(   //
	    &(c->xcn->base),                // xc
	    &timeline_semaphore_handle_raw, // out_handle
	    &xcsem);                        // out_xcsem
	if (xret != XRT_SUCCESS) {
		D3D_WARN(c, "Native compositor tried but failed to created a timeline semaphore for us.");
		return;
	}
	D3D_INFO(c, "Native compositor created a timeline semaphore for us.");

	// Because importFence throws on failure we use this ref.
	unique_compositor_semaphore_ref timeline_semaphore{xcsem};

	// Try to import the fence.
	wil::com_ptr<ID3D11Fence> fence = import_fence(*(c->fence_device), timeline_semaphore_handle_raw);

	// And try to signal the fence to make sure it works.
	HRESULT hr = c->fence_context->Signal(fence.get(), c->timeline_semaphore_value);
	if (!SUCCEEDED(hr)) {
		D3D_WARN(c,
		         "Your graphics driver does not support importing the native compositor's "
		         "semaphores into D3D11, falling back to local blocking.");
		return;
	}

	D3D_INFO(c, "We imported a timeline semaphore and can signal it.");
	// OK, keep these resources around.
	c->fence = std::move(fence);
	c->timeline_semaphore = std::move(timeline_semaphore);
	// c->timeline_semaphore_handle = std::move(timeline_semaphore_handle);
}

static void
client_d3d11_compositor_init_try_internal_blocking(struct client_d3d11_compositor *c)
{
	wil::com_ptr<ID3D11Fence> fence;
	HRESULT hr = c->fence_device->CreateFence( //
	    0,                                     // InitialValue
	    D3D11_FENCE_FLAG_NONE,                 // Flags
	    __uuidof(ID3D11Fence),                 // ReturnedInterface
	    fence.put_void());                     // ppFence

	if (!SUCCEEDED(hr)) {
		char buf[kErrorBufSize];
		formatMessage(hr, buf);
		D3D_WARN(c, "Cannot even create an ID3D11Fence for internal use: %s", buf);
		return;
	}

	hr = c->local_wait_event.create();
	if (!SUCCEEDED(hr)) {
		char buf[kErrorBufSize];
		formatMessage(hr, buf);
		D3D_ERROR(c, "Error creating event for synchronization usage: %s", buf);
		return;
	}

	D3D_INFO(c, "We created our own ID3D11Fence and will wait on it ourselves.");
	c->fence = std::move(fence);
}

struct xrt_compositor_d3d11 *
client_d3d11_compositor_create(struct xrt_compositor_native *xcn, ID3D11Device *device)
try {
	std::unique_ptr<struct client_d3d11_compositor> c = std::make_unique<struct client_d3d11_compositor>();
	c->log_level = debug_get_log_option_log();
	c->xcn = xcn;

	wil::com_ptr<ID3D11Device> app_dev{device};
	if (!app_dev.try_query_to(c->app_device.put())) {
		U_LOG_E("Could not get d3d11 device!");
		return nullptr;
	}
	c->app_device->GetImmediateContext3(c->app_context.put());

	wil::com_ptr<IDXGIAdapter> adapter;

	THROW_IF_FAILED(app_dev.query<IDXGIDevice>()->GetAdapter(adapter.put()));

	{
		// Now, try to get an equivalent device of our own
		wil::com_ptr<ID3D11Device> our_dev;
		wil::com_ptr<ID3D11DeviceContext> our_context;
		std::tie(our_dev, our_context) = xrt::auxiliary::d3d::d3d11::createDevice(adapter, c->log_level);
		our_dev.query_to(c->comp_device.put());
		our_context.query_to(c->comp_context.put());
	}

	// Upcast fence to context version 4 and reference fence device.
	{
		c->app_device.query_to(c->fence_device.put());
		c->app_context.query_to(c->fence_context.put());
	}

	// See if we can make a "timeline semaphore", also known as ID3D11Fence
	client_d3d11_compositor_init_try_timeline_semaphores(c.get());
	if (!c->timeline_semaphore) {
		// OK native compositor doesn't know how to handle timeline semaphores, or we can't import them, but we
		// can still use them entirely internally.
		client_d3d11_compositor_init_try_internal_blocking(c.get());
	}
	if (!c->fence) {
		D3D_WARN(c, "No sync mechanism for D3D11 was successful!");
	}
	c->base.base.get_swapchain_create_properties = client_d3d11_compositor_get_swapchain_create_properties;
	c->base.base.create_swapchain = client_d3d11_create_swapchain;
	c->base.base.create_passthrough = client_d3d11_compositor_passthrough_create;
	c->base.base.create_passthrough_layer = client_d3d11_compositor_passthrough_layer_create;
	c->base.base.destroy_passthrough = client_d3d11_compositor_passthrough_destroy;
	c->base.base.begin_session = client_d3d11_compositor_begin_session;
	c->base.base.end_session = client_d3d11_compositor_end_session;
	c->base.base.wait_frame = client_d3d11_compositor_wait_frame;
	c->base.base.begin_frame = client_d3d11_compositor_begin_frame;
	c->base.base.discard_frame = client_d3d11_compositor_discard_frame;
	c->base.base.layer_begin = client_d3d11_compositor_layer_begin;
	c->base.base.layer_projection = client_d3d11_compositor_layer_projection;
	c->base.base.layer_projection_depth = client_d3d11_compositor_layer_projection_depth;
	c->base.base.layer_quad = client_d3d11_compositor_layer_quad;
	c->base.base.layer_cube = client_d3d11_compositor_layer_cube;
	c->base.base.layer_cylinder = client_d3d11_compositor_layer_cylinder;
	c->base.base.layer_equirect1 = client_d3d11_compositor_layer_equirect1;
	c->base.base.layer_equirect2 = client_d3d11_compositor_layer_equirect2;
	c->base.base.layer_passthrough = client_d3d11_compositor_layer_passthrough;
	c->base.base.layer_commit = client_d3d11_compositor_layer_commit;
	c->base.base.destroy = client_d3d11_compositor_destroy;


	// Passthrough our formats from the native compositor to the client.
	uint32_t count = 0;
	for (uint32_t i = 0; i < xcn->base.info.format_count; i++) {
		// Can we turn this format into DXGI?
		DXGI_FORMAT f = d3d_vk_format_to_dxgi(xcn->base.info.formats[i]);
		if (f == 0) {
			continue;
		}
		// And back to Vulkan?
		auto v = d3d_dxgi_format_to_vk(f);
		if (v == 0) {
			continue;
		}
		// Do we have a typeless version of it?
		DXGI_FORMAT typeless = d3d_dxgi_format_to_typeless_dxgi(f);
		if (typeless == f) {
			continue;
		}

		c->base.base.info.formats[count++] = f;
	}
	c->base.base.info.format_count = count;

	return &(c.release()->base);
} catch (wil::ResultException const &e) {
	U_LOG_E("Error creating D3D11 client compositor: %s", e.what());
	return nullptr;
} catch (std::exception const &e) {
	U_LOG_E("Error creating D3D11 client compositor: %s", e.what());
	return nullptr;
} catch (...) {
	U_LOG_E("Error creating D3D11 client compositor");
	return nullptr;
}

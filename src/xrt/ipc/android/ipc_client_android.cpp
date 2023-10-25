// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementation exposing Android-specific IPC client code to C.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup ipc_android
 */

#include "ipc_client_android.h"

#include "org.freedesktop.monado.ipc.hpp"

#include "xrt/xrt_compositor.h"
#include "xrt/xrt_config_android.h"
#include "util/u_logging.h"
#include "util/u_misc.h"

#include "android/android_load_class.hpp"

#include "wrap/android.app.h"

using wrap::android::app::Activity;
using wrap::org::freedesktop::monado::ipc::Client;
using xrt::auxiliary::android::loadClassFromRuntimeApk;

struct ipc_client_android
{
	ipc_client_android(jobject act) : activity(act) {}
	~ipc_client_android();

	Activity activity{};
	Client client{nullptr};
};

struct android_surface_swapchain
{
	struct xrt_swapchain_native base;

	struct ipc_client_android *ica;

	jobject android_surface;
};

ipc_client_android::~ipc_client_android()
{

	// Tell Java that native code is done with this.
	try {
		if (!client.isNull()) {
			client.markAsDiscardedByNative();
		}
	} catch (std::exception const &e) {
		// Must catch and ignore any exceptions in the destructor!
		U_LOG_E("Failure while marking IPC client as discarded: %s", e.what());
	}
}

struct ipc_client_android *
ipc_client_android_create(struct _JavaVM *vm, void *activity)
{

	jni::init(vm);
	try {
		auto clazz = loadClassFromRuntimeApk((jobject)activity, Client::getFullyQualifiedTypeName());
		if (clazz.isNull()) {
			U_LOG_E("Could not load class '%s' from package '%s'", Client::getFullyQualifiedTypeName(),
			        XRT_ANDROID_PACKAGE);
			return nullptr;
		}

		// Teach the wrapper our class before we start to use it.
		Client::staticInitClass((jclass)clazz.object().getHandle());
		std::unique_ptr<ipc_client_android> ret = std::make_unique<ipc_client_android>((jobject)activity);

		ret->client = Client::construct(ret.get());

		return ret.release();
	} catch (std::exception const &e) {

		U_LOG_E("Could not start IPC client class: %s", e.what());
		return nullptr;
	}
}

int
ipc_client_android_blocking_connect(struct ipc_client_android *ica)
{
	try {
		int fd = ica->client.blockingConnect(ica->activity, XRT_ANDROID_PACKAGE);
		return fd;
	} catch (std::exception const &e) {
		// Must catch and ignore any exceptions in the destructor!
		U_LOG_E("Failure while connecting to IPC server: %s", e.what());
		return -1;
	}
}


void
ipc_client_android_destroy(struct ipc_client_android **ptr_ica)
{

	if (ptr_ica == NULL) {
		return;
	}
	struct ipc_client_android *ica = *ptr_ica;
	if (ica == NULL) {
		return;
	}
	delete ica;
	*ptr_ica = NULL;
}

static void
ipc_client_android_release_android_surface(struct xrt_swapchain *xsc)
{
	struct android_surface_swapchain *assc = (struct android_surface_swapchain *)(xsc);
	struct ipc_client_android *ica = assc->ica;
	try {
		ica->client.releaseSurface((long)(assc));
		jni::env()->DeleteGlobalRef(assc->android_surface);
	} catch (std::exception const &e) {
		U_LOG_E("Failure to get android surface: %s", e.what());
	}
	U_LOG_I("ipc_client_android_release_android_surface");
	free(xsc);
}


void
ipc_client_android_acquire_android_surface(
    int width, int height, struct ipc_client_android *ica, struct xrt_swapchain **out_xsc, void *out_surface)
{
	struct android_surface_swapchain *assc = U_TYPED_CALLOC(struct android_surface_swapchain);
	assc->ica = ica;
	assc->base.base.destroy = ipc_client_android_release_android_surface;
	struct xrt_swapchain *xsc = &assc->base.base;
	try {
		xsc->is_client = true;
		jni::Object surface_obj = ica->client.acquireSurface((long)(assc), width, height);
		assc->android_surface = jni::env()->NewGlobalRef(surface_obj.getHandle());
		*out_xsc = xsc;
		U_LOG_I("assc->android_surface = 0x%lx", (long)(assc->android_surface));
		*(uint64_t *)out_surface = (uint64_t)assc->android_surface;
	} catch (std::exception const &e) {
		out_surface = NULL;
		*out_xsc = NULL;
		U_LOG_E("Failure to get android surface: %s", e.what());
	}
	U_LOG_D("get android surface by out_surface = 0x%lx", (long)(*out_surface));
}

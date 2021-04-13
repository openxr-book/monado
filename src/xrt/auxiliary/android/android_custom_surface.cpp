// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementation of native code for Android custom surface.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * @ingroup aux_android
 */

#include "android_custom_surface.h"
#include "android_surface_callbacks.h"
#include "android_load_class.hpp"

#include "xrt/xrt_config_android.h"
#include "util/u_logging.h"

#include "wrap/android.app.h"
#include "wrap/android.content.h"
#include "wrap/android.view.h"
#include "org.freedesktop.monado.auxiliary.hpp"

#include <android/native_window_jni.h>


using wrap::android::app::Activity;
using wrap::android::view::Surface;
using wrap::android::content::Context;
using wrap::android::view::SurfaceHolder;
using wrap::org::freedesktop::monado::auxiliary::MonadoView;
using xrt::auxiliary::android::loadClassFromRuntimeApk;


struct android_custom_surface
{
	explicit android_custom_surface(jobject act);
	~android_custom_surface();
	Activity activity{};
	MonadoView monadoView{};
	jni::Class monadoViewClass{};
	struct android_surface_callbacks *asc;
};


android_custom_surface::android_custom_surface(jobject act) : activity(act) {}

android_custom_surface::~android_custom_surface()
{
	android_surface_callbacks_destroy(&asc);
	// Tell Java that native code is done with this.
	try {
		if (!monadoView.isNull()) {
			monadoView.markAsDiscardedByNative();
		}
	} catch (std::exception const &e) {
		// Must catch and ignore any exceptions in the destructor!
		U_LOG_E("Failure while marking MonadoView as discarded: %s", e.what());
	}
}

constexpr auto FULLY_QUALIFIED_CLASSNAME = "org.freedesktop.monado.auxiliary.MonadoView";

extern "C" JNIEXPORT void JNICALL
Java_org_freedesktop_monado_auxiliary_MonadoView_surfaceCreatedNative(JNIEnv *env, jobject thiz, jobject surface_holder)
{
	jni::init(env);
	auto holder = SurfaceHolder{surface_holder};
	Surface surface = holder.getSurface();

	ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface.object().getHandle());
	auto custom_surface = static_cast<android_custom_surface *>(MonadoView{thiz}.getNativePointer());
	int callbacks = android_surface_callbacks_invoke(custom_surface->asc, (struct _ANativeWindow *)nativeWindow,
	                                                 XRT_ANDROID_SURFACE_EVENT_ACQUIRED);
	U_LOG_W("Told %d callbacks about acquiring a surface", callbacks);
}

extern "C" JNIEXPORT void JNICALL
Java_org_freedesktop_monado_auxiliary_MonadoView_surfaceDestroyedNative(JNIEnv *env,
                                                                        jobject thiz,
                                                                        jobject surface_holder)
{
	jni::init(env);
	auto holder = SurfaceHolder{surface_holder};
	Surface surface = holder.getSurface();

	ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface.object().getHandle());
	auto custom_surface = static_cast<android_custom_surface *>(MonadoView{thiz}.getNativePointer());
	int callbacks = android_surface_callbacks_invoke(custom_surface->asc, (struct _ANativeWindow *)nativeWindow,
	                                                 XRT_ANDROID_SURFACE_EVENT_LOST);
	U_LOG_W("Told %d callbacks about losing a surface", callbacks);
}

struct android_custom_surface *
android_custom_surface_async_start(struct _JavaVM *vm, void *activity)
{
	jni::init(vm);
	try {
		auto clazz = loadClassFromRuntimeApk((jobject)activity, MonadoView::getFullyQualifiedTypeName());
		if (clazz.isNull()) {
			U_LOG_E("Could not load class '%s' from package '%s'", MonadoView::getFullyQualifiedTypeName(),
			        XRT_ANDROID_PACKAGE);
			return nullptr;
		}

		// Teach the wrapper our class before we start to use it.
		MonadoView::staticInitClass((jclass)clazz.object().getHandle());
		std::unique_ptr<android_custom_surface> ret =
		    std::make_unique<android_custom_surface>((jobject)activity);

		// the 0 is to avoid this being considered "temporary" and to
		// create a global ref.
		ret->monadoViewClass = jni::Class((jclass)clazz.object().getHandle(), 0);

		if (ret->monadoViewClass.isNull()) {
			U_LOG_E("monadoViewClass was null");
			return nullptr;
		}

		std::string clazz_name = ret->monadoViewClass.getName();
		if (clazz_name != MonadoView::getFullyQualifiedTypeName()) {
			U_LOG_E("Unexpected class name: %s", clazz_name.c_str());
			return nullptr;
		}

		ret->monadoView = MonadoView::attachToActivity(ret->activity, ret.get());

		//! @todo instance?
		ret->asc = android_surface_callbacks_create(nullptr);
		return ret.release();
	} catch (std::exception const &e) {

		U_LOG_E("Could not start attaching our custom surface to activity: %s", e.what());
		return nullptr;
	}
}


void
android_custom_surface_destroy(struct android_custom_surface **ptr_custom_surface)
{
	if (ptr_custom_surface == NULL) {
		return;
	}
	struct android_custom_surface *custom_surface = *ptr_custom_surface;
	if (custom_surface == NULL) {
		return;
	}
	delete custom_surface;
	*ptr_custom_surface = NULL;
}

ANativeWindow *
android_custom_surface_wait_get_surface(struct android_custom_surface *custom_surface, uint64_t timeout_ms)
{
	SurfaceHolder surfaceHolder{};
	try {
		surfaceHolder = custom_surface->monadoView.waitGetSurfaceHolder(timeout_ms);

	} catch (std::exception const &e) {
		// do nothing right now.
		U_LOG_E("Could not wait for our custom surface: %s", e.what());
		return nullptr;
	}

	if (surfaceHolder.isNull()) {
		return nullptr;
	}
	auto surf = surfaceHolder.getSurface();
	if (surf.isNull()) {
		return nullptr;
	}
	return ANativeWindow_fromSurface(jni::env(), surf.object().makeLocalReference());
}

bool
android_custom_surface_get_display_metrics(struct _JavaVM *vm,
                                           void *context,
                                           struct xrt_android_display_metrics *out_metrics)
{
	jni::init(vm);

	try {
		auto clazz = loadClassFromRuntimeApk((jobject)context, MonadoView::getFullyQualifiedTypeName());
		if (clazz.isNull()) {
			U_LOG_E("Could not load class '%s' from package '%s'", MonadoView::getFullyQualifiedTypeName(),
			        XRT_ANDROID_PACKAGE);
			return false;
		}

		// Teach the wrapper our class before we start to use it.
		MonadoView::staticInitClass((jclass)clazz.object().getHandle());

		jni::Object displayMetrics = MonadoView::getDisplayMetrics(Context((jobject)context));
		//! @todo implement non-deprecated codepath for api 30+
		float displayRefreshRate = MonadoView::getDisplayRefreshRate(Context((jobject)context));
		if (displayRefreshRate == 0.0) {
			displayRefreshRate = 60.0f;
		}

		*out_metrics = {.width_pixels = displayMetrics.get<int>("widthPixels"),
		                .height_pixels = displayMetrics.get<int>("heightPixels"),
		                .density_dpi = displayMetrics.get<int>("densityDpi"),
		                .density = displayMetrics.get<float>("xdpi"),
		                .scaled_density = displayMetrics.get<float>("ydpi"),
		                .xdpi = displayMetrics.get<float>("density"),
		                .ydpi = displayMetrics.get<float>("scaledDensity"),
		                .refresh_rate = displayRefreshRate};
		return true;
	} catch (std::exception const &e) {
		U_LOG_E("Could not get display metrics: %s", e.what());
		return false;
	}
}
int
android_custom_surface_register_callback(struct android_custom_surface *custom_surface,
                                         xrt_android_surface_event_handler_t callback,
                                         enum xrt_android_surface_event event_mask,
                                         void *userdata)
{
	return android_surface_callbacks_register_callback(custom_surface->asc, callback, event_mask, userdata);
}

int
android_custom_surface_remove_callback(struct android_custom_surface *custom_surface,
                                       xrt_android_surface_event_handler_t callback,
                                       enum xrt_android_surface_event event_mask,
                                       void *userdata)
{
	return android_surface_callbacks_remove_callback(custom_surface->asc, callback, event_mask, userdata);
}

// Copyright 2020-2021, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Inline implementations for partially-generated wrapper for the
 * `org.freedesktop.monado.auxiliary` Java package - do not include on its own!
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup aux_android
 */

#pragma once

#include "wrap/android.app.h"
#include "wrap/android.content.h"
#include "wrap/android.view.h"
#include <vector>


namespace wrap {
namespace org::freedesktop::monado::auxiliary {
	inline MonadoView
	MonadoView::attachToWindow(android::content::Context const &displayContext,
	                           void *nativePointer,
	                           wrap::android::view::WindowManager_LayoutParams const &lp)
	{
		return MonadoView(Meta::data().clazz().call<jni::Object>(
		    Meta::data().attachToWindow, displayContext.object(),
		    static_cast<long long>(reinterpret_cast<uintptr_t>(nativePointer)), lp.object()));
	}

	inline void
	MonadoView::removeFromWindow(const MonadoView &view)
	{
		Meta::data().clazz().call<void>(Meta::data().removeFromWindow, view.object());
	}

	inline jni::Object
	MonadoView::getDisplayMetrics(android::content::Context const &context)
	{
		return Meta::data().clazz().call<jni::Object>(Meta::data().getDisplayMetrics, context.object());
	}

	inline float
	MonadoView::getDisplayRefreshRate(android::content::Context const &context)
	{
		return Meta::data().clazz().call<float>(Meta::data().getDisplayRefreshRate, context.object());
	}

	inline int32_t
	MonadoView::getDisplayModeIdWidth(const android::content::Context &displayContext,
	                                  int32_t displayId,
	                                  int32_t displayModeId)
	{
		return Meta::data().clazz().call<int32_t>(Meta::data().getDisplayModeIdWidth, displayContext.object(),
		                                          displayId, displayModeId);
	}

	inline int32_t
	MonadoView::getDisplayModeIdHeight(const android::content::Context &displayContext,
	                                   int32_t displayId,
	                                   int32_t displayModeId)
	{
		return Meta::data().clazz().call<int32_t>(Meta::data().getDisplayModeIdHeight, displayContext.object(),
		                                          displayId, displayModeId);
	}

	inline std::vector<float>
	MonadoView::getSupportedRefreshRates(android::content::Context const &context)
	{
		jni::Object refreshRateArray =
		    Meta::data().clazz().call<jni::Object>(Meta::data().getSupportedRefreshRates, context.object());
		jfloat *refreshRates =
		    (jfloat *)jni::env()->GetFloatArrayElements((jfloatArray)refreshRateArray.getHandle(), 0);
		jsize length = jni::env()->GetArrayLength((jfloatArray)refreshRateArray.getHandle());
		std::vector<float> refreshRateVector;
		for (int i = 0; i < length; i++) {
			refreshRateVector.push_back(refreshRates[i]);
		}
		return refreshRateVector;
	}

	inline void *
	MonadoView::getNativePointer()
	{
		assert(!isNull());
		return reinterpret_cast<void *>(
		    static_cast<intptr_t>(object().call<long long>(Meta::data().getNativePointer)));
	}

	inline void
	MonadoView::markAsDiscardedByNative()
	{
		assert(!isNull());
		return object().call<void>(Meta::data().markAsDiscardedByNative);
	}

	inline android::view::SurfaceHolder
	MonadoView::waitGetSurfaceHolder(int32_t wait_ms)
	{
		assert(!isNull());
		return android::view::SurfaceHolder(
		    object().call<jni::Object>(Meta::data().waitGetSurfaceHolder, wait_ms));
	}

} // namespace org::freedesktop::monado::auxiliary
} // namespace wrap

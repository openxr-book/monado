// Copyright 2023, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file surface_swapchain_manager.cpp
 * @brief native helper class to maintain texture
 */
#include <algorithm>
#include "surface_swapchain_manager.h"
#include "util/u_logging.h"
#include <jni.h>

void
NativeSurfaceSwapchainManager::addTextureId(int32_t textureId)
{
	auto it = std::find(mFreeTextureIds.begin(), mFreeTextureIds.end(), textureId);
	if (it != mFreeTextureIds.end()) {
		U_LOG_W("add a same texture id = %d", textureId);
	} else {
		mFreeTextureIds.push_back(textureId);
	}
}

int
NativeSurfaceSwapchainManager::getTextureId(uint64_t identity)
{
	U_LOG_I("getTextureIdFromSwapchainId with identity =0x%lx", identity);
	int texurteId = -1;
	if (mSurfaceSwapchainManager != NULL) {
		texurteId = mSurfaceSwapchainManager.getTextureId(identity);
	}
	U_LOG_I("getTextureIdByIdentity of texture id = %d", texurteId);
	return texurteId;
}


void
NativeSurfaceSwapchainManager::updateTexImage(int32_t textureId)
{
	U_LOG_I("updateTexImage with textureId =%d", textureId);
	if (mSurfaceSwapchainManager != NULL) {
		mSurfaceSwapchainManager.updateTexImage(textureId);
	}
}

int32_t
NativeSurfaceSwapchainManager::acquireTextureId()
{
	int32_t textureId = 0;
	if (!mFreeTextureIds.empty()) {
		textureId = mFreeTextureIds[0];
		mUsedTextureIds.push_back(textureId);
		mFreeTextureIds.erase(mFreeTextureIds.begin());
	} else {
		U_LOG_W("all ( size = %d) the texture id has been used  a same texture id = %d",
		        mUsedTextureIds.size());
	}
	return textureId;
}

void
NativeSurfaceSwapchainManager::releaseTextureId(int32_t textureId)
{
	auto it = std::find(mUsedTextureIds.begin(), mUsedTextureIds.end(), textureId);
	if (it != mUsedTextureIds.end()) {
		mFreeTextureIds.push_back(textureId);
		mUsedTextureIds.erase(it);
	} else {
		U_LOG_W("textureId size = %d has not beed used", textureId);
	}
}

void
NativeSurfaceSwapchainManager::setJavaSurfaceSwapchainManager(jobject surfaceSwapchainManager)
{
	mSurfaceSwapchainManager = (SurfaceSwapchainManager)surfaceSwapchainManager;
}

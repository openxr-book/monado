// Copyright 2023, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0

#pragma once
#include <vector>
#include <jni.h>
#include "org.freedesktop.monado.ipc.hpp"

using wrap::org::freedesktop::monado::ipc::SurfaceSwapchainManager;

class NativeSurfaceSwapchainManager
{
public:
	static NativeSurfaceSwapchainManager &
	getInstance()
	{
		static NativeSurfaceSwapchainManager instance;
		return instance;
	}
	NativeSurfaceSwapchainManager() = default;
	void
	setJavaSurfaceSwapchainManager(jobject surfaceSwapchainManager);
	void
	addTextureId(int32_t textureId);
	void
	updateTexImage(int32_t textureId);

	int32_t
	acquireTextureId();
	void
	releaseTextureId(int32_t textureId);
	int
	getTextureId(uint64_t identity);

private:
	std::vector<int32_t> mFreeTextureIds;
	std::vector<int32_t> mUsedTextureIds;
	SurfaceSwapchainManager mSurfaceSwapchainManager;
};

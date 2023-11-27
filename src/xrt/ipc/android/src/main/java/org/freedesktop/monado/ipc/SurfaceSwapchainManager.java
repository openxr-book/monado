// Copyright 2023, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementation of the Android Surface Swapchain manager * @ingroup ipc_android
 */


package org.freedesktop.monado.ipc;

import android.graphics.SurfaceTexture;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

@Keep
public class SurfaceSwapchainManager {

    private static final String TAG = "SurfaceSwapchainManager";

    public SurfaceSwapchainManager() {

    }

    private List<AndroidSwapChainSurfaceTexture> mAndroidSurfaceTextureList = new ArrayList<>();

    public Surface acquireSurface(long identity, int width, int height) {
        Log.v(TAG, "acquireSurface with (w,h) = (" + width + "," + height + ") , identity = 0x" + Long.toHexString(identity));
        if (identity == 0) {
            Log.e(TAG, "acquireSurface fail with identifier = 0x" + Long.toHexString(identity));
            return null;
        }
        int textureId = nativeAcquireTextureId();
        AndroidSwapChainSurfaceTexture androidSwapChainSurfaceTexture = new AndroidSwapChainSurfaceTexture(textureId);
        androidSwapChainSurfaceTexture.setDefaultBufferSize(width, height);
        Surface surface = new Surface(androidSwapChainSurfaceTexture);
        androidSwapChainSurfaceTexture.setSurface(surface);
        androidSwapChainSurfaceTexture.setIdentity(identity);

        mAndroidSurfaceTextureList.add(androidSwapChainSurfaceTexture);
        Log.v(TAG, "acquireSurface success with " + surface + " and " + androidSwapChainSurfaceTexture);
        return surface;
    }

    public void releaseSurface(long identity) {
        Log.v(TAG, "releaseSurface identifier = 0x" + Long.toHexString(identity));
        for (AndroidSwapChainSurfaceTexture surfaceTexture : mAndroidSurfaceTextureList) {
        Log.v(TAG, "compare to:" + surfaceTexture);
            if (identity == surfaceTexture.mIdentityFromClient) {
                Log.v(TAG, "release, textureId = " + surfaceTexture.mTextureId);
                nativeReleaseTextureId(surfaceTexture.mTextureId);
                surfaceTexture.release();
            }
        }
    }

    public int getTextureId(long identity) {
        Log.v(TAG, "getTextureId identity = 0x" + Long.toHexString(identity));
        int textureId = -1;
        for (AndroidSwapChainSurfaceTexture surfaceTexture : mAndroidSurfaceTextureList) {
            Log.v(TAG, "compare to:" + surfaceTexture);
            if (identity == surfaceTexture.mIdentityFromClient) {
                Log.v(TAG, "getTextureId, textureId = " + surfaceTexture.mTextureId);
                textureId = surfaceTexture.mTextureId;
                break;
            }
        }
        return textureId;
    }

    public void updateTexImage(int textureId) {
        Log.v(TAG, "try to updateTexImage textureId = " + textureId + ", list size = "  + mAndroidSurfaceTextureList.size());
        for (AndroidSwapChainSurfaceTexture surfaceTexture : mAndroidSurfaceTextureList) {
            Log.v(TAG, "compare to:" + surfaceTexture);
            if (textureId == surfaceTexture.mTextureId && surfaceTexture.isFrameAvailable) {
                Log.v(TAG, "updateTexImage, textureId = " + textureId);
                surfaceTexture.updateTexImage();
                break;
            }
        }
    }

    public void releaseAllAndroidSurface() {
        Log.v(TAG, "releaseAllAndroidSurface");
        for (AndroidSwapChainSurfaceTexture surfaceTexture : mAndroidSurfaceTextureList) {
            nativeReleaseTextureId(surfaceTexture.mTextureId);
            surfaceTexture.release();
        }
    }

    @SuppressWarnings("JavaJniMissingFunction")
    private native int nativeAcquireTextureId();

    /**
     * Native method that release texture.
     */
    @SuppressWarnings("JavaJniMissingFunction")
    private native void nativeReleaseTextureId(int textureId);

    private class AndroidSwapChainSurfaceTexture extends SurfaceTexture implements SurfaceTexture.OnFrameAvailableListener {
        private long mIdentityFromClient;
        private int mTextureId;
        private boolean isFrameAvailable;
        private Surface mSurface;

        public AndroidSwapChainSurfaceTexture(int textureId) {
            super(textureId);
            mTextureId = textureId;
            isFrameAvailable = false;
            mSurface = null;
            this.setOnFrameAvailableListener(this);
        }

        public void setSurface(Surface surface) {
            mSurface = surface;
        }

        public void setIdentity(long identifier) {
            mIdentityFromClient = identifier;
        }

        @Override
        public void release() {
            Log.v(TAG, "release -> mTextureId = " + mTextureId);
            if (mSurface != null) {
                mSurface.release();
                mSurface = null;
            }
            mTextureId = -1;
            super.release();
        }

        @Override
        public void onFrameAvailable(SurfaceTexture surfaceTexture) {
            Log.v(TAG, "onFrameAvailable -> mTextureId = " + mTextureId + ", identity = 0x" + Long.toHexString(mIdentityFromClient));
            isFrameAvailable = true;
        }

        @Override
        public String toString() {
            return "AndroidSwapChainSurfaceTexture (id = " + mTextureId
                        + ", identity = 0x" + Long.toHexString(mIdentityFromClient)
                        + ",available = " + isFrameAvailable
                        + ") with " + mSurface;
            }
    }
}

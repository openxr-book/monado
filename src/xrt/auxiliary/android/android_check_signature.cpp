// Copyright 2024, Qualcomm Innovation Center, Inc.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Functions for Android app signature check.
 * @author Zhisheng Lv
 * @ingroup aux_android
 */

#include "android_check_signature.h"
#include <wrap/android.app.h>
#include <wrap/android.content.h>
#include <wrap/android.content.pm.h>
#include "jni.h"

#include "util/u_logging.h"
#include <string>

using wrap::android::content::Context;
using wrap::android::content::pm::Signature;
using wrap::android::content::pm::PackageInfo;
using wrap::android::content::pm::PackageManager;


bool android_check_signature(void *application_context, const char *runtime_package_name){
    if(runtime_package_name == nullptr){
        U_LOG_E("android_check_signature: runtime_package_name is null");
        return false;
    }
    try{
        auto context = Context{(jobject)application_context};

        if (context.isNull()) {
            U_LOG_E("android_check_signature: application_context was null");
            return false;
        }
        std::string appPackageName = context.getPackageName();
        U_LOG_I("android_check_signature: appPackageName: %s", appPackageName.c_str());
        auto packageManager = PackageManager{context.getPackageManager()};
        if (packageManager.isNull()) {
            U_LOG_E("android_check_signature: application_context.getPackageManager() returned null");
            return false;
        }
        
        auto appPackageInfo = packageManager.getPackageInfo(appPackageName, 64);
        if (appPackageInfo.isNull()) {
            U_LOG_E("android_check_signature: packageManager.getPackageInfo() returned null");
            return false;
        }
        auto appSignature = appPackageInfo.getSignature();
        if (appSignature.isNull()) {
            U_LOG_E("android_check_signature: appPackageInfo.getSignature() returned null");
            return false;
        }
        std::string appSig = appSignature.toCharsString();

        U_LOG_I("android_check_signature: runtimePackageName: %s", runtime_package_name);
        auto runtimePackageInfo = packageManager.getPackageInfo(std::string(runtime_package_name), 64);
        
        if (runtimePackageInfo.isNull()) {
            U_LOG_E("android_check_signature: packageManager.getPackageInfo() returned null");
            return false;
        }

        auto runtimeSignature = runtimePackageInfo.getSignature();
        if (runtimeSignature.isNull()) {
            U_LOG_E("android_check_signature: appPackageInfo.getSignature() returned null");
            return false;
        }
        std::string runtimeSig = runtimeSignature.toCharsString();
        return appSig == runtimeSig;
    } catch (std::exception const &e) {
        U_LOG_E("jni exception info: %s", e.what());
        return false;
    }
}
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
#include <sys/system_properties.h>
#include "jni.h"

#include "util/u_logging.h"

using wrap::android::content::Context;
using wrap::android::content::pm::PackageInfo;
using wrap::android::content::pm::PackageManager;
using wrap::android::content::pm::Signature;


std::string
android_get_app_signature(void *application_context, const char *app_package_name)
{
	if (application_context == nullptr) {
		U_LOG_E("%s: context is null", __func__);
		return std::string();
	}
	if (app_package_name == nullptr) {
		U_LOG_E("%s: app_package_name is null", __func__);
		return std::string();
	}
	try {
		auto context = Context{(jobject)application_context};
		if (context.isNull()) {
			U_LOG_E("%s: application_context was null", __func__);
			return std::string();
		}
		auto packageManager = PackageManager{context.getPackageManager()};
		if (packageManager.isNull()) {
			U_LOG_E("%s: application_context.getPackageManager() returned null", __func__);
			return std::string();
		}
		auto appPackageInfo = packageManager.getPackageInfo(std::string(app_package_name), 64);
		if (appPackageInfo.isNull()) {
			U_LOG_E("%s: packageManager.getPackageInfo() returned null", __func__);
			return std::string();
		}
		auto appSignature = appPackageInfo.getSignature();
		if (appSignature.isNull()) {
			U_LOG_E("%s: appPackageInfo.getSignature() returned null", __func__);
			return std::string();
		}
		return appSignature.toCharsString();
	} catch (std::exception const &e) {
		U_LOG_E("%s: jni exception info: %s", __func__, e.what());
		return std::string();
	}
}

bool
android_check_signature(void *application_context, const char *runtime_package_name)
{
	if (runtime_package_name == nullptr) {
		U_LOG_E("%s: runtime_package_name is null", __func__);
		return false;
	}
	try {
		auto context = Context{(jobject)application_context};

		if (context.isNull()) {
			U_LOG_E("%s: application_context was null", __func__);
			return false;
		}
		std::string appPackageName = context.getPackageName();
		U_LOG_I("%s: appPackageName: %s", __func__, appPackageName.c_str());
		std::string appSig = android_get_app_signature(application_context, appPackageName.c_str());
		U_LOG_I("%s: runtimePackageName: %s", __func__, runtime_package_name);
		std::string runtimeSig = android_get_app_signature(application_context, runtime_package_name);

		if (runtimeSig.empty()) {
			U_LOG_E("%s: runtime signature is empty", __func__);
			return false;
		}
		return appSig == runtimeSig;
	} catch (std::exception const &e) {
		U_LOG_E("%s: jni exception info: %s", __func__, e.what());
		return false;
	}
}

bool
is_extension_enabled(const unsigned int enabled_extension_count,
                     const char *const *enabled_extension_names,
                     const char *target_extension_name)
{
	if (enabled_extension_count == 0 || enabled_extension_names == NULL || target_extension_name == NULL) {
		return false;
	} else {
		for (int i = 0; i < enabled_extension_count; ++i) {
			if (!strncmp(enabled_extension_names[i], target_extension_name,
			             strlen(target_extension_name))) {
				return true;
			}
		}
	}
	return false;
}

bool
is_check_overlay_signature_property_enabled()
{
	const char propName[] = "debug.openxr.runtime.checkOverlaySignature";
	char propValue[PROP_VALUE_MAX] = {0};
	const char propValueTrue[] = "true";
	if ((0 != __system_property_get(propName, propValue)) &&
	    (0 == strncmp(propValue, propValueTrue, strlen(propValueTrue)))) {
		return true;
	}
	return false;
}
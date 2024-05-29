// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementations for loading Java code from a package.
 * @author Rylie Pavlik <rylie.pavlik@collabora.com>
 * @ingroup aux_android
 */

#include "android_load_class.hpp"

#include "util/u_logging.h"

#include "wrap/android.content.h"
#include "wrap/android.content.pm.h"
#include "wrap/dalvik.system.h"

#include "jni.h"

#include <dlfcn.h>

using wrap::android::content::Context;
using wrap::android::content::pm::ApplicationInfo;
using wrap::android::content::pm::PackageManager;
using wrap::dalvik::system::DexClassLoader;

namespace xrt::auxiliary::android {

static constexpr char kIntentAction[] = "org.khronos.openxr.OpenXRRuntimeService";

/*!
 * Hacky way to retrieve runtime source dir.
 */
static std::string
getRuntimeSourceDir()
{
	Dl_info info{};
	std::string dir;
	if (dladdr((void *)&getRuntimeSourceDir, &info)) {
		// dli_filename is full path of the library contains the symbol. For example:
		// /data/app/~~sha27MVNR46wLF-96zA_LQ==/org.freedesktop.monado.openxr_runtime.out_of_process-cqs8L2Co3WfHGgvDwF12JA==/lib/arm64/libopenxr_monado.so
		dir = info.dli_fname;
		dir = dir.substr(0, dir.find("/lib/"));

		// /data/app/~~sha27MVNR46wLF-96zA_LQ==/org.freedesktop.monado.openxr_runtime.out_of_process-cqs8L2Co3WfHGgvDwF12JA==/base.apk!/lib/arm64/libopenxr_monado.so
		dir = dir.substr(0, dir.find("/base.apk!"));
	}

	return dir;
}

ApplicationInfo
getAppInfo(std::string const &packageName, jobject application_context)
{
	try {
		auto context = Context{application_context};
		if (context.isNull()) {
			U_LOG_E("getAppInfo: application_context was null");
			return {};
		}
		auto packageManager = PackageManager{context.getPackageManager()};
		if (packageManager.isNull()) {
			U_LOG_E(
			    "getAppInfo: "
			    "application_context.getPackageManager() returned null");
			return {};
		}
		auto intent = wrap::android::content::Intent::construct(kIntentAction);
		auto resolutions = packageManager.queryIntentServices(
		    intent, PackageManager::GET_META_DATA | PackageManager::GET_SHARED_LIBRARY_FILES);
		if (resolutions.isNull() || resolutions.size() == 0) {
			U_LOG_E(
			    "getAppInfo: "
			    "application_context.getPackageManager().queryIntentServices() returned null or empty");
			return {};
		}
		const auto n = resolutions.size();
		ApplicationInfo appInfo;
		for (int32_t i = 0; i < n; ++i) {
			wrap::android::content::pm::ResolveInfo resolution{resolutions.get(i)};
			auto service = resolution.getServiceInfo();
			if (service.isNull()) {
				continue;
			}
			U_LOG_I("getAppInfo: Considering package %s", service.getPackageName().c_str());
			if (service.getPackageName() == packageName) {
				appInfo = service.getApplicationInfo();
				break;
			}
		}

		if (appInfo.isNull()) {
			U_LOG_E(
			    "getAppInfo: "
			    "could not find a package advertising the intent %s named %s",
			    kIntentAction, packageName.c_str());
			return {};
		}
		return appInfo;
	} catch (std::exception const &e) {
		U_LOG_E("Could not get App Info: %s", e.what());
		return {};
	}
}

wrap::java::lang::Class
loadClassFromPackage(ApplicationInfo applicationInfo, jobject application_context, const char *clazz_name)
{
	auto context = Context{application_context}.getApplicationContext();
	auto pkgContext = context.createPackageContext(
	    applicationInfo.getPackageName(), Context::CONTEXT_IGNORE_SECURITY | Context::CONTEXT_INCLUDE_CODE);

	// Not using ClassLoader.loadClass because it expects a /-delimited
	// class name, while we have a .-delimited class name.
	// This does work
	wrap::java::lang::ClassLoader pkgClassLoader = pkgContext.getClassLoader();

	try {
		auto loadedClass = pkgClassLoader.loadClass(std::string(clazz_name));
		if (loadedClass.isNull()) {
			U_LOG_E("Could not load class for name %s", clazz_name);
			return wrap::java::lang::Class();
		}

		return loadedClass;
	} catch (std::exception const &e) {
		U_LOG_E("Could not load class '%s' forName: %s", clazz_name, e.what());
		return wrap::java::lang::Class();
	}
}

wrap::java::lang::Class
loadClassFromApk(jobject application_context, const char *apk_path, const char *clazz_name)
{
	Context context = Context{application_context}.getApplicationContext();
	DexClassLoader classLoader = DexClassLoader::construct(apk_path, "", context.getClassLoader().object());
	try {
		auto loadedClass = classLoader.loadClass(std::string(clazz_name));
		if (loadedClass.isNull()) {
			U_LOG_E("Could not load class for name %s from %s", clazz_name, apk_path);
			return wrap::java::lang::Class();
		}

		return loadedClass;
	} catch (std::exception const &e) {
		U_LOG_E("Could not load class '%s' from '%s' forName: %s", clazz_name, apk_path, e.what());
		return wrap::java::lang::Class();
	}
}

wrap::java::lang::Class
loadClassFromRuntimeApk(jobject application_context, const char *clazz_name)
{
	if (!application_context) {
		U_LOG_E("Could not load class %s, invalid context", clazz_name);
		return {};
	}

	std::string runtimeApkPath = getRuntimeSourceDir() + "/base.apk";
	return loadClassFromApk(application_context, runtimeApkPath.c_str(), clazz_name);
}

} // namespace xrt::auxiliary::android


void *
android_load_class_from_package(struct _JavaVM *vm,
                                const char *pkgname,
                                void *application_context,
                                const char *classname)
{
	using namespace xrt::auxiliary::android;
	jni::init(vm);
	Context context((jobject)application_context);
	auto info = getAppInfo(pkgname, (jobject)application_context);
	if (info.isNull()) {
		U_LOG_E("Could not get application info for package '%s'", pkgname);
		return nullptr;
	}
	auto clazz = loadClassFromPackage(info, (jobject)application_context, classname);
	if (clazz.isNull()) {
		U_LOG_E("Could not load class '%s' from package '%s'", classname, pkgname);
		return nullptr;
	}
	return clazz.object().getHandle();
}

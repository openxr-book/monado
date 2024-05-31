// Copyright 2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Implementation of android_environment.h
 * @author Korcan Hussein <korcan.hussein@collabora.com>
 * @ingroup aux_android
 */
#include "android_environment.h"
#include "wrap/ObjectWrapperBase.h"
#include "util/u_logging.h"

#include <string>

namespace {
;

struct File final : public wrap::ObjectWrapperBase
{

	File(jni::Object obj) : wrap::ObjectWrapperBase(obj) {}

	using wrap::ObjectWrapperBase::ObjectWrapperBase;
	static constexpr const char *
	getTypeName() noexcept
	{
		return "java/io/File";
	}

	/*!
	 * Class metadata
	 */
	struct Meta final : public wrap::MetaBaseDroppable
	{
		jni::method_t getAbsolutePath;

		/*!
		 * Singleton accessor
		 */
		static Meta &
		data()
		{
			static Meta instance{};
			return instance;
		}

	private:
		Meta()
		    : MetaBaseDroppable(File::getTypeName()),
		      getAbsolutePath(classRef().getMethod("getAbsolutePath", "()Ljava/lang/String;"))
		{
			MetaBaseDroppable::dropClassRef();
		}
	};

	std::string
	getAbsolutePath()
	{
		assert(!isNull());
		return object().call<std::string>(Meta::data().getAbsolutePath);
	}
};

class Environment final : public wrap::ObjectWrapperBase
{
public:
	using wrap::ObjectWrapperBase::ObjectWrapperBase;
	static constexpr const char *
	getTypeName() noexcept
	{
		return "android/os/Environment";
	}

	/*!
	 * Class metadata
	 */
	struct Meta final : public wrap::MetaBase
	{
		jni::method_t getExternalStorageDirectory;

		/*!
		 * Singleton accessor
		 */
		static Meta &
		data()
		{
			static Meta instance{};
			return instance;
		}

	private:
		Meta()
		    : MetaBase(Environment::getTypeName()), getExternalStorageDirectory(classRef().getStaticMethod(
		                                                "getExternalStorageDirectory", "()Ljava/io/File;"))
		{}
	};

	static File
	getExternalStorageDirectory()
	{
		return Meta::data().clazz().call<jni::Object>(Meta::data().getExternalStorageDirectory);
	}
};
}; // namespace

bool
android_enviroment_get_external_storage_dir(char *str, size_t size)
{
	try {
		if (size == 0 || str == nullptr) {
			throw std::invalid_argument("Dst string is null or zero buffer size");
		}
		File file = Environment::getExternalStorageDirectory();
		if (file.isNull()) {
			throw std::runtime_error("Failed to get File object");
		}
		const std::string dirPath = file.getAbsolutePath();
		if (size < (dirPath.length() + 1)) {
			throw std::length_error("Dst string length to small");
		}
		dirPath.copy(str, dirPath.length());
		str[dirPath.length()] = '\0';
		return true;
	} catch (std::exception const &e) {
		U_LOG_E("Could not get external storage directory path: %s", e.what());
		return false;
	}
}
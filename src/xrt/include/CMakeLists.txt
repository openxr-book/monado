# Copyright 2020, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

add_subdirectory(xrt)

add_library(xrt-interfaces INTERFACE)
target_include_directories(
	xrt-interfaces INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
	)

if(XRT_FEATURE_OPENXR_BODY_TRACKING_FULL_BODY_META)
	set(OCULUS_MOBILE_OPENXR_SDK_URL
	    "https://securecdn.oculus.com/binaries/download/?id=7577210995650755"
		)
	include(FetchContent)
	if(CMAKE_VERSION VERSION_LESS "3.24.0")
		FetchContent_Declare(
			oculussdk
			URL ${OCULUS_MOBILE_OPENXR_SDK_URL}
			    CONFIGURE_COMMAND
			    ""
			    BUILD_COMMAND
			    ""
			    EXCLUDE_FROM_ALL
			)
	else()
		FetchContent_Declare(
			oculussdk
			URL ${OCULUS_MOBILE_OPENXR_SDK_URL}
			    CONFIGURE_COMMAND
			    ""
			    BUILD_COMMAND
			    ""
			    DOWNLOAD_EXTRACT_TIMESTAMP
			    TRUE
			    EXCLUDE_FROM_ALL
			)
	endif()
	FetchContent_GetProperties(oculussdk)
	if(NOT oculussdk_POPULATED)
		FetchContent_Populate(oculussdk)
	endif()

	add_library(oculussdk INTERFACE)
	target_include_directories(oculussdk INTERFACE "${oculussdk_SOURCE_DIR}/OpenXR")

	target_link_libraries(xrt-interfaces INTERFACE oculussdk)
	target_compile_definitions(xrt-interfaces INTERFACE XRT_USE_META_EXT_HEADERS)
endif()

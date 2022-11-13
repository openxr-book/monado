# Copyright 2020-2022, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0

add_executable(
	sdl-test
	sdl_compositor.c
	sdl_device.c
	sdl_hack_stubs.c
	sdl_instance.c
	sdl_internal.h
	sdl_internal.hpp
	sdl_main.c
	sdl_program.cpp
	sdl_swapchain.c
	)
add_sanitizers(sdl-test)

target_link_libraries(
	sdl-test
	PRIVATE
		xrt-external-imgui-sdl2
		aux_os
		aux_ogl
		aux_util
		comp_util
		comp_multi
		ipc_server
		st_gui
		drv_includes
		drv_simulated
		OpenFace::LandmarkDetector 
		OpenFace::FaceAnalyser 
		OpenFace::GazeAnalyser 
		OpenFace::Utilities 
		dlib 
		${OpenCV_LIBS} ${TBB_LIBRARIES} ${BLAS_LIBRARIES}
	)

if(WIN32)
	target_link_libraries(gui PRIVATE SDL2::SDL2main)
endif()

install(TARGETS sdl-test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

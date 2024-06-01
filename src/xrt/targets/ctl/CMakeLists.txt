# Copyright 2020-2021, Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0


# Declare the library target
add_library(monado-ctl-gui SHARED
    monado-ctl-gui.c
    monado-ctl-gui.h
)

target_include_directories(monado-ctl-gui PRIVATE ipc)
target_link_libraries(monado-ctl-gui PRIVATE aux_util ipc_client)

set_target_properties(monado-ctl-gui PROPERTIES PUBLIC_HEADER ctl.h)


add_executable(monado-ctl main.c)
add_sanitizers(monado-ctl)

target_include_directories(monado-ctl PRIVATE ipc)

target_link_libraries(monado-ctl PRIVATE aux_util ipc_client)

install(TARGETS monado-ctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

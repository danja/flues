cmake_minimum_required(VERSION 3.10)
project(achord VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

set(SOURCES
    src/grid_state.c
    src/midi_comm.c
    src/chord_map.c
    src/voicing_engine.c
    src/achord_engine.c
    src/achord_plugin.cpp
)

add_library(achord SHARED ${SOURCES})
target_link_libraries(achord m)
set_target_properties(achord PROPERTIES
    PREFIX ""
    OUTPUT_NAME "achord"
    SUFFIX ".so"
)

add_library(achord_ui SHARED
    src/ui/achord_ui_x11.c
)
target_link_libraries(achord_ui
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    pthread
)
set_target_properties(achord_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "achord_ui"
    SUFFIX ".so"
)

set(LV2_INSTALL_DIR achord.lv2)

install(TARGETS achord achord_ui
    LIBRARY DESTINATION ${LV2_INSTALL_DIR}
)

install(FILES
    achord.lv2/manifest.ttl
    achord.lv2/achord.ttl
    DESTINATION ${LV2_INSTALL_DIR}
)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "LV2 install dir: ${CMAKE_INSTALL_PREFIX}/${LV2_INSTALL_DIR}")

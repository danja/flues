cmake_minimum_required(VERSION 3.10)
project(padseq VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")

# Find LV2
find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

# Find X11 and Cairo for UI
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/grid_state.c
    src/midi_comm.c
    src/padseq_engine.c
    src/padseq_plugin.cpp
)

# Build shared library
add_library(padseq SHARED ${SOURCES})

target_link_libraries(padseq
    m
)

# LV2 plugin naming convention (no "lib" prefix, .so extension)
set_target_properties(padseq PROPERTIES
    PREFIX ""
    OUTPUT_NAME "padseq"
    SUFFIX ".so"
)

# Build UI library
add_library(padseq_ui SHARED
    src/ui/padseq_ui_x11.c
)

target_link_libraries(padseq_ui
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    pthread
)

set_target_properties(padseq_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "padseq_ui"
    SUFFIX ".so"
)

# Install plugin
set(LV2_INSTALL_DIR padseq.lv2)

install(TARGETS padseq padseq_ui
    LIBRARY DESTINATION ${LV2_INSTALL_DIR}
)

install(FILES
    padseq.lv2/manifest.ttl
    padseq.lv2/padseq.ttl
    DESTINATION ${LV2_INSTALL_DIR}
)

# Print configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "LV2 install dir: ${CMAKE_INSTALL_PREFIX}/${LV2_INSTALL_DIR}")

cmake_minimum_required(VERSION 3.16)
project(flues_control_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find LV2
find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

# DSP plugin (C++ - no custom UI)
add_library(flues_control MODULE src/flues_control_plugin.cpp)
target_include_directories(flues_control PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(flues_control PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(flues_control PRIVATE ${LV2_LIBRARIES})
set_target_properties(flues_control PROPERTIES
    PREFIX ""
    OUTPUT_NAME "flues_control"
)

# UI plugin (X11/Cairo)
add_library(flues_control_ui MODULE src/ui/flues_control_ui_x11.c)
target_include_directories(flues_control_ui
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(flues_control_ui
    PRIVATE
        ${LV2_LIBRARIES}
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        m
        pthread
)
target_compile_options(flues_control_ui PRIVATE
    ${LV2_CFLAGS_OTHER}
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)
target_compile_definitions(flues_control_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(flues_control_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "flues_control_ui"
)

# Install to ~/.lv2/flues-control.lv2/
install(TARGETS flues_control
    LIBRARY DESTINATION flues-control.lv2
)
install(TARGETS flues_control_ui
    LIBRARY DESTINATION flues-control.lv2
)
install(FILES
    flues-control.lv2/manifest.ttl
    flues-control.lv2/flues-control.ttl
    DESTINATION flues-control.lv2
)

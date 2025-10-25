cmake_minimum_required(VERSION 3.16)
project(floozy_dev_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(floozy_dev MODULE
    src/floozy_plugin.cpp
)

target_include_directories(floozy_dev
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
        ../pm-synth/src
        ../pm-synth/src/modules
        ../pm-synth/src/modules/interface
        ../pm-synth/src/modules/interface/utils
        ../disyn/src
        ../disyn/src/modules
)

target_compile_definitions(floozy_dev PRIVATE LV2_EXPORT_SHARED)

target_link_libraries(floozy_dev PRIVATE ${LV2_LIBRARIES})
target_compile_options(floozy_dev PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(floozy_dev PROPERTIES
    PREFIX ""
    OUTPUT_NAME "floozy-dev"
)

add_library(floozy_dev_ui MODULE
    src/ui/floozy_ui_x11.c
)

target_include_directories(floozy_dev_ui
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        src/ui
)

target_link_libraries(floozy_dev_ui
    PRIVATE
        ${LV2_LIBRARIES}
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        m
        pthread
)

target_compile_options(floozy_dev_ui PRIVATE
    ${LV2_CFLAGS_OTHER}
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

target_compile_definitions(floozy_dev_ui PRIVATE LV2_EXPORT_SHARED)

set_target_properties(floozy_dev_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "floozy-dev_ui"
)

install(TARGETS floozy_dev
    LIBRARY DESTINATION floozy-dev.lv2
)

install(TARGETS floozy_dev_ui
    LIBRARY DESTINATION floozy-dev.lv2
)

install(FILES
    floozy-dev.lv2/manifest.ttl
    floozy-dev.lv2/floozy.ttl
    DESTINATION floozy-dev.lv2
)

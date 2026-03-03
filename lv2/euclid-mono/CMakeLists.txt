cmake_minimum_required(VERSION 3.16)
project(euclid_mono_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(euclid_mono MODULE src/euclid_mono_plugin.cpp)
target_include_directories(euclid_mono PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(euclid_mono PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(euclid_mono PRIVATE ${LV2_LIBRARIES})
set_target_properties(euclid_mono PROPERTIES
    PREFIX ""
    OUTPUT_NAME "euclid_mono"
)

add_library(euclid_mono_ui MODULE src/ui/euclid_mono_ui_x11.c)
target_include_directories(euclid_mono_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(euclid_mono_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(euclid_mono_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(euclid_mono_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "euclid_mono_ui"
)

install(TARGETS euclid_mono euclid_mono_ui
    LIBRARY DESTINATION euclid-mono.lv2
)
install(FILES
    euclid-mono.lv2/manifest.ttl
    euclid-mono.lv2/euclid-mono.ttl
    DESTINATION euclid-mono.lv2
)

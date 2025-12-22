cmake_minimum_required(VERSION 3.16)
project(euclid_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(euclid MODULE src/euclid_plugin.cpp)
target_include_directories(euclid PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(euclid PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(euclid PRIVATE ${LV2_LIBRARIES})
set_target_properties(euclid PROPERTIES
    PREFIX ""
    OUTPUT_NAME "euclid"
)

# UI plugin (C)
add_library(euclid_ui MODULE src/ui/euclid_ui_x11.c)
target_include_directories(euclid_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(euclid_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(euclid_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(euclid_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "euclid_ui"
)

install(TARGETS euclid euclid_ui
    LIBRARY DESTINATION euclid.lv2
)
install(FILES
    euclid.lv2/manifest.ttl
    euclid.lv2/euclid.ttl
    DESTINATION euclid.lv2
)

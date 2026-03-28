cmake_minimum_required(VERSION 3.16)
project(bassgen_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(bassgen MODULE src/bassgen_plugin.cpp)
target_include_directories(bassgen PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(bassgen PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(bassgen PRIVATE ${LV2_LIBRARIES})
set_target_properties(bassgen PROPERTIES
    PREFIX ""
    OUTPUT_NAME "bassgen"
)

add_library(bassgen_ui MODULE src/ui/bassgen_ui_x11.c)
target_include_directories(bassgen_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(bassgen_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(bassgen_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(bassgen_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "bassgen_ui"
)

install(TARGETS bassgen bassgen_ui
    LIBRARY DESTINATION bassgen.lv2
)
install(FILES
    bassgen.lv2/manifest.ttl
    bassgen.lv2/bassgen.ttl
    DESTINATION bassgen.lv2
)

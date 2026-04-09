cmake_minimum_required(VERSION 3.16)
project(drumgen_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(drumgen MODULE src/drumgen_plugin.cpp)
target_include_directories(drumgen PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(drumgen PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(drumgen PRIVATE ${LV2_LIBRARIES})
set_target_properties(drumgen PROPERTIES
    PREFIX ""
    OUTPUT_NAME "drumgen"
)

add_library(drumgen_ui MODULE src/ui/drumgen_ui_x11.c)
target_include_directories(drumgen_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(drumgen_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(drumgen_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(drumgen_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "drumgen_ui"
)

install(TARGETS drumgen drumgen_ui
    LIBRARY DESTINATION drumgen.lv2
)
install(FILES
    drumgen.lv2/manifest.ttl
    drumgen.lv2/drumgen.ttl
    DESTINATION drumgen.lv2
)

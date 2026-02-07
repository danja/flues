cmake_minimum_required(VERSION 3.16)
project(slimmer_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(slimmer MODULE src/slimmer_plugin.cpp)
target_include_directories(slimmer PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(slimmer PRIVATE LV2_EXPORT_SHARED)
set_target_properties(slimmer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "slimmer")

add_library(slimmer_ui MODULE src/ui/slimmer_ui_x11.c)
target_include_directories(slimmer_ui PRIVATE ${LV2_INCLUDE_DIRS})
target_link_libraries(slimmer_ui PRIVATE ${LV2_LIBRARIES} X11 cairo m pthread)
target_compile_definitions(slimmer_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(slimmer_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "slimmer_ui")

install(TARGETS slimmer slimmer_ui
    LIBRARY DESTINATION slimmer.lv2)

install(FILES
    slimmer.lv2/manifest.ttl
    slimmer.lv2/slimmer.ttl
    DESTINATION slimmer.lv2)

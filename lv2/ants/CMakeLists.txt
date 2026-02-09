cmake_minimum_required(VERSION 3.16)
project(ants_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(ants MODULE src/ants_plugin.cpp)
target_include_directories(ants PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(ants PRIVATE LV2_EXPORT_SHARED)
set_target_properties(ants PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ants")

add_library(ants_ui MODULE src/ui/ants_ui_x11.c)
target_include_directories(ants_ui PRIVATE ${LV2_INCLUDE_DIRS})
target_link_libraries(ants_ui PRIVATE ${LV2_LIBRARIES} X11 cairo m pthread)
target_compile_definitions(ants_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(ants_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ants_ui")

install(TARGETS ants ants_ui
    LIBRARY DESTINATION ants.lv2)

install(FILES
    ants.lv2/manifest.ttl
    ants.lv2/ants.ttl
    DESTINATION ants.lv2)

cmake_minimum_required(VERSION 3.16)
project(quantico_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(quantico MODULE src/quantico_plugin.cpp)
target_include_directories(quantico PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(quantico PRIVATE LV2_EXPORT_SHARED)
set_target_properties(quantico PROPERTIES
    PREFIX ""
    OUTPUT_NAME "quantico")

add_library(quantico_ui MODULE src/ui/quantico_ui_x11.c)
target_include_directories(quantico_ui PRIVATE ${LV2_INCLUDE_DIRS})
target_link_libraries(quantico_ui PRIVATE ${LV2_LIBRARIES} X11 cairo m pthread)
target_compile_definitions(quantico_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(quantico_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "quantico_ui")

install(TARGETS quantico quantico_ui
    LIBRARY DESTINATION quantico.lv2)

install(FILES
    quantico.lv2/manifest.ttl
    quantico.lv2/quantico.ttl
    DESTINATION quantico.lv2)

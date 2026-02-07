cmake_minimum_required(VERSION 3.16)
project(metalv_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(LILV REQUIRED lilv-0)

add_library(metalv MODULE
    src/metalv_plugin.cpp
    src/host/HostEngine.cpp
    src/mcp/MCPServer.cpp)

target_include_directories(metalv PRIVATE ${LV2_INCLUDE_DIRS} ${LILV_INCLUDE_DIRS})
target_link_libraries(metalv PRIVATE ${LV2_LIBRARIES} ${LILV_LIBRARIES} pthread)
target_compile_definitions(metalv PRIVATE LV2_EXPORT_SHARED)
set_target_properties(metalv PROPERTIES
    PREFIX ""
    OUTPUT_NAME "metalv")

add_library(metalv_ui MODULE src/ui/metalv_ui_x11.c)
target_include_directories(metalv_ui PRIVATE ${LV2_INCLUDE_DIRS})
target_link_libraries(metalv_ui PRIVATE ${LV2_LIBRARIES} X11 cairo m pthread)
target_compile_definitions(metalv_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(metalv_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "metalv_ui")

install(TARGETS metalv metalv_ui
    LIBRARY DESTINATION metalv.lv2)

install(FILES
    metalv.lv2/manifest.ttl
    metalv.lv2/metalv.ttl
    DESTINATION metalv.lv2)

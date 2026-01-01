cmake_minimum_required(VERSION 3.16)
project(memone_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(memone MODULE src/memone_plugin.cpp)
target_include_directories(memone PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
    src/modules
)
target_compile_definitions(memone PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(memone PRIVATE ${LV2_LIBRARIES})
set_target_properties(memone PROPERTIES
    PREFIX ""
    OUTPUT_NAME "memone"
)

# UI plugin (C)
add_library(memone_ui MODULE src/ui/memone_ui_x11.c)
target_include_directories(memone_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(memone_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(memone_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(memone_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "memone_ui"
)

install(TARGETS memone memone_ui
    LIBRARY DESTINATION memone.lv2
)
install(FILES
    memone.lv2/manifest.ttl
    memone.lv2/memone.ttl
    DESTINATION memone.lv2
)

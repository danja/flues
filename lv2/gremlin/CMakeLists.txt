cmake_minimum_required(VERSION 3.16)
project(gremlin_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(gremlin MODULE
    src/gremlin_plugin.cpp
)

target_include_directories(gremlin
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
)

target_compile_definitions(gremlin PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(gremlin PRIVATE ${LV2_LIBRARIES})
target_compile_options(gremlin PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(gremlin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gremlin"
)

add_library(gremlin_ui MODULE
    src/ui/gremlin_ui_x11.c
)

target_include_directories(gremlin_ui
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        src/ui
)

target_link_libraries(gremlin_ui
    PRIVATE
        ${LV2_LIBRARIES}
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        m
        pthread
)

target_compile_options(gremlin_ui PRIVATE
    ${LV2_CFLAGS_OTHER}
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

target_compile_definitions(gremlin_ui PRIVATE LV2_EXPORT_SHARED)

set_target_properties(gremlin_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gremlin_ui"
)

install(TARGETS gremlin
    LIBRARY DESTINATION gremlin.lv2
)

install(TARGETS gremlin_ui
    LIBRARY DESTINATION gremlin.lv2
)

install(FILES
    gremlin.lv2/manifest.ttl
    gremlin.lv2/gremlin.ttl
    DESTINATION gremlin.lv2
)

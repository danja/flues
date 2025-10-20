cmake_minimum_required(VERSION 3.16)
project(disyn_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(disyn MODULE
    src/disyn_plugin.cpp
)

target_include_directories(disyn
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
)

target_sources(disyn
    PRIVATE
        src/DisynEngine.hpp
        src/modules/OscillatorModule.hpp
        src/modules/EnvelopeModule.hpp
        src/modules/ReverbModule.hpp
)

target_compile_definitions(disyn PRIVATE LV2_EXPORT_SHARED)

target_link_libraries(disyn PRIVATE ${LV2_LIBRARIES})
target_compile_options(disyn PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(disyn PROPERTIES
    PREFIX ""
    OUTPUT_NAME "disyn"
)

install(TARGETS disyn
    LIBRARY DESTINATION disyn.lv2
)

add_library(disyn_ui MODULE
    src/ui/disyn_ui_x11.c
)

target_include_directories(disyn_ui
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        src/ui
)

target_link_libraries(disyn_ui
    PRIVATE
        ${LV2_LIBRARIES}
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        m
        pthread
)

target_compile_options(disyn_ui PRIVATE
    ${LV2_CFLAGS_OTHER}
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

target_compile_definitions(disyn_ui PRIVATE LV2_EXPORT_SHARED)

set_target_properties(disyn_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "disyn_ui"
)

install(FILES
    disyn.lv2/manifest.ttl
    disyn.lv2/disyn.ttl
    DESTINATION disyn.lv2
)

install(TARGETS disyn_ui
    LIBRARY DESTINATION disyn.lv2
)

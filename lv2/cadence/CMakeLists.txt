cmake_minimum_required(VERSION 3.16)
project(cadence_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(cadence MODULE
    src/cadence_plugin.cpp
    src/cadence_comping.cpp
    src/cadence_harmony.cpp
    src/cadence_transport.cpp
    src/cadence_variation.cpp
    src/cadence_state.cpp
)
target_include_directories(cadence PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(cadence PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(cadence PRIVATE ${LV2_LIBRARIES})
set_target_properties(cadence PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cadence"
)

add_library(cadence_ui MODULE src/ui/cadence_ui_x11.c)
target_include_directories(cadence_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    src
)
target_link_libraries(cadence_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(cadence_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(cadence_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cadence_ui"
)

install(TARGETS cadence cadence_ui
    LIBRARY DESTINATION cadence.lv2
)
install(FILES
    cadence.lv2/manifest.ttl
    cadence.lv2/cadence.ttl
    DESTINATION cadence.lv2
)

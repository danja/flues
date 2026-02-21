cmake_minimum_required(VERSION 3.16)
project(e_mix_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(e_mix MODULE src/e_mix_plugin.cpp)
target_include_directories(e_mix PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(e_mix PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(e_mix PRIVATE ${LV2_LIBRARIES})
set_target_properties(e_mix PROPERTIES
    PREFIX ""
    OUTPUT_NAME "e_mix"
)

add_library(e_mix_ui MODULE src/ui/e_mix_ui_x11.c)
target_include_directories(e_mix_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(e_mix_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(e_mix_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(e_mix_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "e_mix_ui"
)

install(TARGETS e_mix e_mix_ui
    LIBRARY DESTINATION e-mix.lv2
)
install(FILES
    e-mix.lv2/manifest.ttl
    e-mix.lv2/e-mix.ttl
    DESTINATION e-mix.lv2
)

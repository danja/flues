cmake_minimum_required(VERSION 3.16)
project(chatgen_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

# DSP plugin (C++)
add_library(chatgen MODULE src/chatgen_plugin.cpp)
target_include_directories(chatgen PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
    src/modules
)
target_compile_definitions(chatgen PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(chatgen PRIVATE ${LV2_LIBRARIES})
set_target_properties(chatgen PROPERTIES
    PREFIX ""
    OUTPUT_NAME "chatgen"
)

# UI plugin (C)
add_library(chatgen_ui MODULE src/ui/chatgen_ui_x11.c)
target_include_directories(chatgen_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(chatgen_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(chatgen_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(chatgen_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "chatgen_ui"
)

# Install to ~/.lv2/chatgen.lv2/
install(TARGETS chatgen chatgen_ui
    LIBRARY DESTINATION chatgen.lv2
)
install(FILES
    chatgen.lv2/manifest.ttl
    chatgen.lv2/chatgen.ttl
    DESTINATION chatgen.lv2
)

cmake_minimum_required(VERSION 3.16)
project(chatterbox_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

# DSP plugin (C++)
add_library(chatterbox MODULE src/chatterbox_plugin.cpp)
target_include_directories(chatterbox PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
    src/modules
    ../pm-synth/src/modules  # Reuse ReverbModule
)
target_compile_definitions(chatterbox PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(chatterbox PRIVATE ${LV2_LIBRARIES})
set_target_properties(chatterbox PROPERTIES
    PREFIX ""
    OUTPUT_NAME "chatterbox"
)

# UI plugin (C)
add_library(chatterbox_ui MODULE src/ui/chatterbox_ui_x11.c)
target_include_directories(chatterbox_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(chatterbox_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(chatterbox_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(chatterbox_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "chatterbox_ui"
)

# Install to ~/.lv2/chatterbox.lv2/
install(TARGETS chatterbox chatterbox_ui
    LIBRARY DESTINATION chatterbox.lv2
)
install(FILES
    chatterbox.lv2/manifest.ttl
    chatterbox.lv2/chatterbox.ttl
    DESTINATION chatterbox.lv2
)

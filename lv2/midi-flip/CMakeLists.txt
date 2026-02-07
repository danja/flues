cmake_minimum_required(VERSION 3.16)
project(midi_flip_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(midi_flip MODULE src/midi_flip_plugin.cpp)
target_include_directories(midi_flip PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(midi_flip PRIVATE LV2_EXPORT_SHARED)
set_target_properties(midi_flip PROPERTIES
    PREFIX ""
    OUTPUT_NAME "midi_flip")

add_library(midi_flip_ui MODULE src/ui/midi_flip_ui_x11.c)
target_include_directories(midi_flip_ui PRIVATE ${LV2_INCLUDE_DIRS})
target_link_libraries(midi_flip_ui PRIVATE ${LV2_LIBRARIES} X11 cairo m pthread)
target_compile_definitions(midi_flip_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(midi_flip_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "midi_flip_ui")

install(TARGETS midi_flip midi_flip_ui
    LIBRARY DESTINATION midi-flip.lv2)

install(FILES
    midi-flip.lv2/manifest.ttl
    midi-flip.lv2/midi-flip.ttl
    DESTINATION midi-flip.lv2)

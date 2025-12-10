cmake_minimum_required(VERSION 3.16)
project(drumkit_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

# DSP Plugin (C++)
add_library(drumkit MODULE
    src/drumkit_plugin.cpp
)

target_include_directories(drumkit
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
        src/modules
        src/voices
)

target_sources(drumkit
    PRIVATE
        src/DrumKitEngine.hpp
        src/modules/NoiseGenerator.hpp
        src/modules/ADEnvelope.hpp
        src/modules/PitchEnvelope.hpp
        src/modules/BiquadFilter.hpp
        src/modules/Distortion.hpp
        src/modules/Bitcrusher.hpp
        src/modules/DCBlocker.hpp
        src/modules/ReverbModule.hpp
        src/voices/KickVoice.hpp
        src/voices/SnareVoice.hpp
        src/voices/ClapVoice.hpp
        src/voices/TomVoice.hpp
        src/voices/HiHatVoice.hpp
        src/voices/CrashVoice.hpp
)

target_compile_definitions(drumkit PRIVATE LV2_EXPORT_SHARED)

target_link_libraries(drumkit PRIVATE ${LV2_LIBRARIES})
target_compile_options(drumkit PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(drumkit PROPERTIES
    PREFIX ""
    OUTPUT_NAME "drumkit"
)

# UI Plugin (C)
add_library(drumkit_ui MODULE
    src/ui/drumkit_ui_x11.c
)

target_include_directories(drumkit_ui
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        src/ui
)

target_link_libraries(drumkit_ui
    PRIVATE
        ${LV2_LIBRARIES}
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        m
        pthread
)

target_compile_options(drumkit_ui PRIVATE
    ${LV2_CFLAGS_OTHER}
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

target_compile_definitions(drumkit_ui PRIVATE LV2_EXPORT_SHARED)

set_target_properties(drumkit_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "drumkit_ui"
)

# Install to ~/.lv2/drumkit.lv2/
install(TARGETS drumkit drumkit_ui
    LIBRARY DESTINATION drumkit.lv2
)

install(FILES
    drumkit.lv2/manifest.ttl
    drumkit.lv2/drumkit.ttl
    DESTINATION drumkit.lv2
)

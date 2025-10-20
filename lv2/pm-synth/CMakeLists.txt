cmake_minimum_required(VERSION 3.16)
project(pm_synth_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(pm_synth MODULE
    src/pm_synth_plugin.cpp
)

target_include_directories(pm_synth
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
)

target_sources(pm_synth
    PRIVATE
        src/PMSynthEngine.hpp
        src/Random.hpp
        src/modules/SourcesModule.hpp
        src/modules/EnvelopeModule.hpp
        src/modules/InterfaceModule.hpp
        src/modules/DelayLinesModule.hpp
        src/modules/FeedbackModule.hpp
        src/modules/FilterModule.hpp
        src/modules/ModulationModule.hpp
        src/modules/ReverbModule.hpp
        src/modules/interface/InterfaceStrategy.hpp
        src/modules/interface/InterfaceFactory.hpp
        src/modules/interface/strategies/PluckStrategy.hpp
        src/modules/interface/strategies/HitStrategy.hpp
        src/modules/interface/strategies/ReedStrategy.hpp
        src/modules/interface/strategies/FluteStrategy.hpp
        src/modules/interface/strategies/BrassStrategy.hpp
        src/modules/interface/strategies/BowStrategy.hpp
        src/modules/interface/strategies/BellStrategy.hpp
        src/modules/interface/strategies/DrumStrategy.hpp
        src/modules/interface/strategies/CrystalStrategy.hpp
        src/modules/interface/strategies/VaporStrategy.hpp
        src/modules/interface/strategies/QuantumStrategy.hpp
        src/modules/interface/strategies/PlasmaStrategy.hpp
        src/modules/interface/utils/NonlinearityLib.hpp
        src/modules/interface/utils/ExcitationGen.hpp
        src/modules/interface/utils/EnergyTracker.hpp
        src/modules/interface/utils/DelayUtils.hpp
)

target_compile_definitions(pm_synth PRIVATE LV2_EXPORT_SHARED)

target_link_libraries(pm_synth PRIVATE ${LV2_LIBRARIES})
target_compile_options(pm_synth PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(pm_synth PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pm_synth"
)

add_library(pm_synth_ui MODULE
    src/ui/pm_synth_ui_x11.c
)

target_include_directories(pm_synth_ui
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${CAIRO_INCLUDE_DIRS}
        src/ui
)

target_link_libraries(pm_synth_ui
    PRIVATE
        ${LV2_LIBRARIES}
        ${X11_LIBRARIES}
        ${CAIRO_LIBRARIES}
        m
        pthread
)

target_compile_options(pm_synth_ui PRIVATE
    ${LV2_CFLAGS_OTHER}
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
)

target_compile_definitions(pm_synth_ui PRIVATE LV2_EXPORT_SHARED)

set_target_properties(pm_synth_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pm_synth_ui"
)

install(TARGETS pm_synth
    LIBRARY DESTINATION pm-synth.lv2
)

install(TARGETS pm_synth_ui
    LIBRARY DESTINATION pm-synth.lv2
)

install(FILES
    pm-synth.lv2/manifest.ttl
    pm-synth.lv2/pm-synth.ttl
    DESTINATION pm-synth.lv2
)

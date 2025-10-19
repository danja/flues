cmake_minimum_required(VERSION 3.16)
project(disyn_lv2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

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

install(FILES
    disyn.lv2/manifest.ttl
    disyn.lv2/disyn.ttl
    DESTINATION disyn.lv2
)

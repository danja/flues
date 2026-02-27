cmake_minimum_required(VERSION 3.16)
project(bubbles_lv2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(bubbles MODULE
    src/bubbles_plugin.cpp
)

target_include_directories(bubbles
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
)

target_sources(bubbles
    PRIVATE
        src/BubblesEngine.hpp
)

target_compile_definitions(bubbles PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(bubbles PRIVATE ${LV2_LIBRARIES})
target_compile_options(bubbles PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(bubbles PROPERTIES
    PREFIX ""
    OUTPUT_NAME "bubbles"
)

install(TARGETS bubbles
    LIBRARY DESTINATION bubbles.lv2
)

install(FILES
    bubbles.lv2/manifest.ttl
    bubbles.lv2/bubbles.ttl
    DESTINATION bubbles.lv2
)

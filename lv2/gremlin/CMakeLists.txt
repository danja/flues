cmake_minimum_required(VERSION 3.16)
project(gremlin_lv2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(gremlin MODULE
    src/gremlin_plugin.cpp
)

target_include_directories(gremlin
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
)

target_compile_definitions(gremlin PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(gremlin PRIVATE ${LV2_LIBRARIES})
target_compile_options(gremlin PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(gremlin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gremlin"
)

install(TARGETS gremlin
    LIBRARY DESTINATION gremlin.lv2
)

install(FILES
    gremlin.lv2/manifest.ttl
    gremlin.lv2/gremlin.ttl
    DESTINATION gremlin.lv2
)

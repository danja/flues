cmake_minimum_required(VERSION 3.16)
project(gremlin_driver_lv2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(gremlin_driver MODULE
    src/gremlin_driver_plugin.cpp
)

target_include_directories(gremlin_driver
    PRIVATE
        ${LV2_INCLUDE_DIRS}
        src
)

target_compile_definitions(gremlin_driver PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(gremlin_driver PRIVATE ${LV2_LIBRARIES})
target_compile_options(gremlin_driver PRIVATE ${LV2_CFLAGS_OTHER})

set_target_properties(gremlin_driver PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gremlin_driver"
)

install(TARGETS gremlin_driver
    LIBRARY DESTINATION gremlin-driver.lv2
)

install(FILES
    gremlin-driver.lv2/manifest.ttl
    gremlin-driver.lv2/gremlin-driver.ttl
    DESTINATION gremlin-driver.lv2
)

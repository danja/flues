cmake_minimum_required(VERSION 3.16)
project(euclidean_gate_lv2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(euclidean_gate MODULE src/euclidean_gate_plugin.cpp)
target_include_directories(euclidean_gate PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(euclidean_gate PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(euclidean_gate PRIVATE ${LV2_LIBRARIES} m)
set_target_properties(euclidean_gate PROPERTIES
    PREFIX ""
    OUTPUT_NAME "euclidean_gate"
)

install(TARGETS euclidean_gate
    LIBRARY DESTINATION euclidean-gate.lv2
)
install(FILES
    euclidean-gate.lv2/manifest.ttl
    euclidean-gate.lv2/euclidean-gate.ttl
    DESTINATION euclidean-gate.lv2
)

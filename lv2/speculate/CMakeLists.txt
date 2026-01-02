cmake_minimum_required(VERSION 3.16)
project(speculate_lv2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

add_library(speculate MODULE src/speculate_plugin.cpp)
target_include_directories(speculate PRIVATE ${LV2_INCLUDE_DIRS})
target_compile_definitions(speculate PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(speculate PRIVATE ${LV2_LIBRARIES} m)
set_target_properties(speculate PROPERTIES
    PREFIX ""
    OUTPUT_NAME "speculate"
)

install(TARGETS speculate
    LIBRARY DESTINATION speculate.lv2
)
install(FILES
    speculate.lv2/manifest.ttl
    speculate.lv2/speculate.ttl
    DESTINATION speculate.lv2
)

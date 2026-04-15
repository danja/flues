cmake_minimum_required(VERSION 3.16)
project(convulse_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(convulse MODULE src/convulse_plugin.cpp)
target_include_directories(convulse PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(convulse PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(convulse PRIVATE
    ${LV2_LIBRARIES}
    m
)
set_target_properties(convulse PROPERTIES
    PREFIX ""
    OUTPUT_NAME "convulse"
)

add_library(convulse_ui MODULE src/ui/convulse_ui_x11.c)
target_include_directories(convulse_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(convulse_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(convulse_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(convulse_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "convulse_ui"
)

install(TARGETS convulse convulse_ui
    LIBRARY DESTINATION convulse.lv2
)
install(FILES
    convulse.lv2/manifest.ttl
    convulse.lv2/convulse.ttl
    DESTINATION convulse.lv2
)

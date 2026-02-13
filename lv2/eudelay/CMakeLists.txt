cmake_minimum_required(VERSION 3.16)
project(eudelay_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(eudelay MODULE src/eudelay_plugin.cpp)
target_include_directories(eudelay PRIVATE
    ${LV2_INCLUDE_DIRS}
    src
)
target_compile_definitions(eudelay PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(eudelay PRIVATE ${LV2_LIBRARIES})
set_target_properties(eudelay PROPERTIES
    PREFIX ""
    OUTPUT_NAME "eudelay"
)

add_library(eudelay_ui MODULE src/ui/eudelay_ui_x11.c)
target_include_directories(eudelay_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(eudelay_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(eudelay_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(eudelay_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "eudelay_ui"
)

install(TARGETS eudelay eudelay_ui
    LIBRARY DESTINATION eudelay.lv2
)
install(FILES
    eudelay.lv2/manifest.ttl
    eudelay.lv2/eudelay.ttl
    DESTINATION eudelay.lv2
)

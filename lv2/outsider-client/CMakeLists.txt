cmake_minimum_required(VERSION 3.16)
project(outsider_client_lv2 VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_library(outsider_client MODULE
    src/outsider_client_plugin.cpp
    src/outsider_client_net.cpp
)
target_include_directories(outsider_client PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../outsider/include
)
target_compile_definitions(outsider_client PRIVATE LV2_EXPORT_SHARED)
target_link_libraries(outsider_client PRIVATE
    ${LV2_LIBRARIES}
    m
    pthread
)
set_target_properties(outsider_client PROPERTIES
    PREFIX ""
    OUTPUT_NAME "outsider_client"
)

add_library(outsider_client_ui MODULE
    src/ui/outsider_client_ui_x11.c
)
target_include_directories(outsider_client_ui PRIVATE
    ${LV2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)
target_link_libraries(outsider_client_ui PRIVATE
    ${LV2_LIBRARIES}
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)
target_compile_definitions(outsider_client_ui PRIVATE LV2_EXPORT_SHARED)
set_target_properties(outsider_client_ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "outsider_client_ui"
)

install(TARGETS outsider_client outsider_client_ui
    LIBRARY DESTINATION outsider-client.lv2
)
install(FILES
    outsider-client.lv2/manifest.ttl
    outsider-client.lv2/outsider-client.ttl
    DESTINATION outsider-client.lv2
)

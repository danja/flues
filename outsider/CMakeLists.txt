cmake_minimum_required(VERSION 3.16)
project(outsider VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(CAIRO REQUIRED cairo)

add_executable(outsider
    src/app/main.cpp
    src/app/outsider_app.cpp
    src/net/ws_server.cpp
    src/session/session_persistence.cpp
    src/session/session_registry.cpp
    src/transport/transport_authority.cpp
    src/semaphore/semaphore_engine.cpp
    src/semaphore/p_mix_model.cpp
    src/semaphore/e_mix_model.cpp
    src/ui/outsider_ui_x11.cpp
)

target_include_directories(outsider PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
)

target_link_libraries(outsider PRIVATE
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
    m
    pthread
)

cmake_minimum_required(VERSION 3.16)
project(stlviewer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3   REQUIRED gtk+-3.0)
pkg_check_modules(EPOXY  REQUIRED epoxy)

add_executable(stlviewer
    src/main.c
    src/stl.c
    src/renderer.c
    src/app.c
)

target_include_directories(stlviewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GTK3_INCLUDE_DIRS}
    ${EPOXY_INCLUDE_DIRS}
)

target_compile_options(stlviewer PRIVATE
    ${GTK3_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_libraries(stlviewer PRIVATE
    ${GTK3_LIBRARIES}
    ${EPOXY_LIBRARIES}
    m
)

install(TARGETS stlviewer RUNTIME DESTINATION bin)

add_subdirectory(thumbnailer)

cmake_minimum_required(VERSION 3.16)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPNG REQUIRED libpng)

# Locate OSMesa (libosmesa6-dev on Ubuntu/Debian)
find_path(OSMESA_INCLUDE_DIR GL/osmesa.h)
find_library(OSMESA_LIB NAMES OSMesa osmesa)
find_library(GL_LIB NAMES GL)

if(NOT OSMESA_INCLUDE_DIR OR NOT OSMESA_LIB OR NOT GL_LIB)
    message(WARNING
        "OSMesa not found — stl-thumbnailer will not be built.\n"
        "Install it with: sudo apt install libosmesa6-dev")
    return()
endif()

message(STATUS "OSMesa found: ${OSMESA_LIB}")

add_executable(stl-thumbnailer
    src/main.c
    src/render.c
    src/png_write.c
    ${CMAKE_SOURCE_DIR}/src/stl.c
)

target_include_directories(stl-thumbnailer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src          # stl.h, math3d.h
    ${OSMESA_INCLUDE_DIR}
    ${LIBPNG_INCLUDE_DIRS}
)

target_compile_options(stl-thumbnailer PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

target_link_libraries(stl-thumbnailer PRIVATE
    ${OSMESA_LIB}
    ${GL_LIB}
    ${LIBPNG_LIBRARIES}
    m
)

install(TARGETS stl-thumbnailer RUNTIME DESTINATION bin)
install(FILES stl.thumbnailer   DESTINATION share/thumbnailers)

cmake_minimum_required(VERSION 3.10)
project(ttyimg C)

set(CMAKE_C_STANDARD 99)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED libdrm)

include_directories(${LIBDRM_INCLUDE_DIRS})
link_directories(${LIBDRM_LIBRARY_DIRS})

add_executable(ttyimg main.c drm.c)

# Explicitly link against drm (in case LIBDRM_LIBRARIES is empty)
target_link_libraries(ttyimg drm m)

# Copy the text image into the binary directory
file(COPY txt.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

cmake_minimum_required(VERSION 3.31)
project(Gallery)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}\\scripts\\buildsystems\\vcpkg.cmake")
    message("Automatically configured vcpkg: ${CMAKE_TOOLCHAIN_FILE}")
else()
    message("Found manually configured vcpkg: ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(CMAKE_CXX_STANDARD 20)
include_directories(include)

find_package(JPEG REQUIRED)
add_executable(image_encode_unit image_encode_unit/ieu.h
        image_encode_unit/ieu.cpp)
target_link_libraries(image_encode_unit PRIVATE JPEG::JPEG)

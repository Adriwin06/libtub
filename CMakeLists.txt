cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(libtub CXX C)

set(LIBTUB_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)

option(LIBTUB_BUILD_TOOLS "Build tools using libtub" OFF)
if(LIBTUB_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

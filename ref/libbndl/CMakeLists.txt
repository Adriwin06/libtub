cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(libbndl CXX C)

set(LIBBNDL_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(src)

option(LIBBNDL_BUILD_TOOLS "Build tools using libbndl" OFF)
if(LIBBNDL_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

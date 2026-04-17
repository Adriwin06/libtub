cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(libtub CXX C)

set(LIBTUB_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

include(CTest)

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

option(LIBTUB_BUILD_TOOLS "Build tools using libtub" OFF)
if(LIBTUB_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

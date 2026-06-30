cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(libtub VERSION 0.1.0 LANGUAGES CXX C)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

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

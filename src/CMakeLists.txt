include(FetchContent)
include(GenerateExportHeader)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(LIBTUB_BUILD_PROJECT_SUPPORT "Build YAML project import/export support" ON)

set(HEADER_DIR ${LIBTUB_ROOT}/include/libtub)
file(GLOB_RECURSE PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${HEADER_DIR}/*.hpp"
    "${HEADER_DIR}/*.h"
    "${HEADER_DIR}/*.inc"
)

set(LIBTUB_CORE_SOURCES
    bundle.cpp
    bundle_c.cpp
    builder.cpp
    formats/base.cpp
    formats/bnd2.cpp
    formats/bndl.cpp
)

set(LIBTUB_PROJECT_SOURCES
    project.cpp
)

set_source_files_properties(${HEADER_DIR}/bundle.h PROPERTIES LANGUAGE C
                                                              HEADER_FILE_ONLY ON)

FetchContent_Declare(
    binaryio
    GIT_REPOSITORY https://github.com/Bo98/libbinaryio
    GIT_TAG        2a95966233e818cf5c0990d6bf338da4afc81c7e
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(
    ZLIB
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG        5a82f71ed1dfc0bec044d9702463dbdf84ea3b71
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml
    GIT_TAG        ee86beb30e4973f5feffe3ce63bfa4fbadf72f38 # v1.15
    SOURCE_SUBDIR  dontbuild
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS 1.11
)

set(ZLIB_BUILD_SHARED ${BUILD_SHARED_LIBS})
set(ZLIB_BUILD_TESTING OFF)
FetchContent_MakeAvailable(binaryio ZLIB pugixml)
if(NOT ZLIB_FOUND AND NOT BUILD_SHARED_LIBS)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

if(LIBTUB_BUILD_PROJECT_SUPPORT)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp
        GIT_TAG        0.8.0
        EXCLUDE_FROM_ALL
        FIND_PACKAGE_ARGS
    )

    set(YAML_CPP_BUILD_CONTRIB OFF)
    set(YAML_CPP_BUILD_TESTS OFF)
    set(YAML_CPP_BUILD_TOOLS OFF)
    FetchContent_MakeAvailable(yaml-cpp)
endif()

function(configure_libtub_library target_name)
    target_link_libraries(${target_name} PRIVATE binaryio ZLIB::ZLIB)
    if(TARGET pugixml::pugixml)
        target_link_libraries(${target_name} PRIVATE pugixml::pugixml)
    else()
        target_include_directories(${target_name} SYSTEM PRIVATE ${pugixml_SOURCE_DIR}/src)
        target_compile_definitions(${target_name} PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
    endif()

    target_compile_features(${target_name} PUBLIC c_std_11 cxx_std_20)
    target_include_directories(${target_name} PUBLIC ${LIBTUB_ROOT}/include ${CMAKE_CURRENT_BINARY_DIR}/include)

    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions(${target_name} PUBLIC LIBTUB_STATIC_DEFINE)
    endif()

    set_target_properties(${target_name} PROPERTIES PREFIX ""
                                                    CXX_VISIBILITY_PRESET hidden
                                                    POSITION_INDEPENDENT_CODE ON
                                                    VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL})

    if(MSVC)
        target_compile_options(${target_name} PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
    endif()
endfunction()

add_library(libtub_core ${LIBTUB_CORE_SOURCES})
configure_libtub_library(libtub_core)
set_target_properties(libtub_core PROPERTIES OUTPUT_NAME "libtub_core")
target_sources(libtub_core PUBLIC FILE_SET HEADERS
                                  BASE_DIRS "${HEADER_DIR}"
                                  FILES "${PUBLIC_HEADERS}")
generate_export_header(libtub_core
    EXPORT_MACRO_NAME LIBTUB_EXPORT
    EXPORT_FILE_NAME include/libtub/internal/export.h
)

set(LIBTUB_INSTALL_TARGETS libtub_core)

if(LIBTUB_BUILD_PROJECT_SUPPORT)
    add_library(libtub ${LIBTUB_CORE_SOURCES} ${LIBTUB_PROJECT_SOURCES})
    configure_libtub_library(libtub)
    target_compile_definitions(libtub PRIVATE libtub_core_EXPORTS)

    if(TARGET yaml-cpp::yaml-cpp)
        target_link_libraries(libtub PRIVATE yaml-cpp::yaml-cpp)
    else()
        target_link_libraries(libtub PRIVATE yaml-cpp)
    endif()

    list(APPEND LIBTUB_INSTALL_TARGETS libtub)
else()
    add_library(libtub ALIAS libtub_core)
endif()

install(TARGETS libtub_core FILE_SET HEADERS)
if(LIBTUB_BUILD_PROJECT_SUPPORT)
    install(TARGETS libtub)
endif()

if(CMAKE_CXX_LINKER_SUPPORTS_PDB AND BUILD_SHARED_LIBS)
    foreach(target_name IN LISTS LIBTUB_INSTALL_TARGETS)
        install(FILES $<TARGET_PDB_FILE:${target_name}> TYPE BIN OPTIONAL)
    endforeach()
endif()

include(GNUInstallDirs) # `TYPE DOC` behaviour changes(!) when GNUInstallDirs is included
install(FILES ${LIBTUB_ROOT}/LICENSE.md TYPE DOC RENAME copyright)
install(FILES ${LIBTUB_ROOT}/NOTICE.libbndl.md TYPE DOC)

if(NOT ZLIB_FOUND AND ZLIB_BUILD_SHARED)
    install(TARGETS zlib ARCHIVE EXCLUDE_FROM_ALL)
endif()

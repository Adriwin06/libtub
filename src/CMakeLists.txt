include(FetchContent)
include(GenerateExportHeader)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(HEADER_DIR ${LIBTUB_ROOT}/include/libtub)
file(GLOB_RECURSE PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${HEADER_DIR}/*.hpp"
    "${HEADER_DIR}/*.h"
    "${HEADER_DIR}/*.inc"
)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "*.cpp")

add_library(libtub ${SRC_FILES})

target_sources(libtub PUBLIC FILE_SET HEADERS
                              BASE_DIRS "${HEADER_DIR}"
                              FILES "${PUBLIC_HEADERS}")
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
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp
    GIT_TAG        0.8.0
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)

set(ZLIB_BUILD_SHARED ${BUILD_SHARED_LIBS})
set(ZLIB_BUILD_TESTING OFF)
set(YAML_CPP_BUILD_CONTRIB OFF)
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
FetchContent_MakeAvailable(binaryio ZLIB pugixml yaml-cpp)
if(NOT ZLIB_FOUND AND NOT BUILD_SHARED_LIBS)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

target_link_libraries(libtub PRIVATE binaryio ZLIB::ZLIB)
if(TARGET yaml-cpp::yaml-cpp)
    target_link_libraries(libtub PRIVATE yaml-cpp::yaml-cpp)
else()
    target_link_libraries(libtub PRIVATE yaml-cpp)
endif()
if(TARGET pugixml::pugixml)
    target_link_libraries(libtub PRIVATE pugixml::pugixml)
else()
    target_include_directories(libtub SYSTEM PRIVATE ${pugixml_SOURCE_DIR}/src)
    target_compile_definitions(libtub PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
endif()

target_compile_features(libtub PUBLIC c_std_11 cxx_std_20)

set_target_properties(libtub PROPERTIES PREFIX ""
                                         CXX_VISIBILITY_PRESET hidden
                                         POSITION_INDEPENDENT_CODE ON
                                         VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL})

generate_export_header(libtub EXPORT_FILE_NAME include/libtub/internal/export.h)

target_include_directories(libtub PUBLIC ${LIBTUB_ROOT}/include ${CMAKE_CURRENT_BINARY_DIR}/include)

if(MSVC)
    target_compile_options(libtub PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

install(TARGETS libtub FILE_SET HEADERS)
if(CMAKE_CXX_LINKER_SUPPORTS_PDB AND BUILD_SHARED_LIBS)
    install(FILES $<TARGET_PDB_FILE:libtub> TYPE BIN OPTIONAL)
endif()

include(GNUInstallDirs) # `TYPE DOC` behaviour changes(!) when GNUInstallDirs is included
install(FILES ${LIBTUB_ROOT}/LICENSE TYPE DOC RENAME copyright)
install(FILES ${LIBTUB_ROOT}/NOTICE.libbndl.md TYPE DOC)

if(NOT ZLIB_FOUND AND ZLIB_BUILD_SHARED)
    install(TARGETS zlib ARCHIVE EXCLUDE_FROM_ALL)
endif()

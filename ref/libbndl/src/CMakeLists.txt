include(FetchContent)
include(GenerateExportHeader)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(HEADER_DIR ${LIBBNDL_ROOT}/include/libbndl)
set(PUBLIC_HEADERS ${HEADER_DIR}/bundle.hpp)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "*.c"
    "*.h"
    "*.hpp"
    "*.cpp"
)

add_library(libbndl ${SRC_FILES} ${PUBLIC_HEADERS})

FetchContent_Declare(
    binaryio
    GIT_REPOSITORY https://github.com/Bo98/libbinaryio
    GIT_TAG        bd46c9df067f67d514fd781ea6dfb1b289d648f8
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(
    ZLIB
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG        5a82f71ed1dfc0bec044d9702463dbdf84ea3b71
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml
    GIT_TAG        ee86beb30e4973f5feffe3ce63bfa4fbadf72f38 # v1.15
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS 1.11
)

set(ZLIB_BUILD_SHARED ${BUILD_SHARED_LIBS})
FetchContent_MakeAvailable(binaryio ZLIB pugixml)
if(NOT ZLIB_FOUND AND NOT BUILD_SHARED_LIBS)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

add_dependencies(libbndl ZLIB::ZLIB)
target_link_libraries(libbndl PRIVATE libbinaryio ZLIB::ZLIB pugixml::pugixml)
target_compile_definitions(libbndl PRIVATE PUGIXML_HEADER_ONLY)

set_property(TARGET libbndl PROPERTY CXX_STANDARD 17)
set_property(TARGET libbndl PROPERTY PREFIX "")
set_property(TARGET libbndl PROPERTY CXX_VISIBILITY_PRESET hidden)

if(NOT BUILD_SHARED_LIBS)
    set_property(TARGET libbndl PROPERTY COMPILE_FLAGS -DLIBBNDL_STATIC_DEFINE)
endif()
generate_export_header(libbndl)

target_include_directories(libbndl PUBLIC ${LIBBNDL_ROOT}/include ${CMAKE_CURRENT_BINARY_DIR})

if(MSVC)
    target_compile_options(libbndl PRIVATE "/permissive-" "/W3" "/Zc:throwingNew" "/EHsc")
endif()

set_property(TARGET libbndl PROPERTY POSITION_INDEPENDENT_CODE ON)

install(TARGETS libbndl RUNTIME DESTINATION bin
                        LIBRARY DESTINATION lib
                        ARCHIVE DESTINATION lib)
if(CMAKE_CXX_LINKER_SUPPORTS_PDB AND BUILD_SHARED_LIBS)
    install(FILES $<TARGET_PDB_FILE:libbndl> DESTINATION bin OPTIONAL)
endif()
install(FILES ${PUBLIC_HEADERS} ${CMAKE_CURRENT_BINARY_DIR}/libbndl_export.h DESTINATION include/libbndl)
install(FILES ${LIBBNDL_ROOT}/LICENSE.md DESTINATION share/libbndl RENAME copyright)

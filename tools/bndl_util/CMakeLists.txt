add_executable(bndl_util main.cpp)

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts
    GIT_TAG        44380e5a44706ab7347f400698c703eb2a196202 # v3.3.1
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(cxxopts)
FetchContent_GetProperties(pugixml)

target_link_libraries(bndl_util PRIVATE libbndl cxxopts::cxxopts)
if(TARGET pugixml::pugixml)
    target_link_libraries(bndl_util PRIVATE pugixml::pugixml)
else()
    target_include_directories(bndl_util SYSTEM PRIVATE ${pugixml_SOURCE_DIR}/src)
    target_compile_definitions(bndl_util PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
endif()

set_property(TARGET bndl_util PROPERTY CXX_STANDARD 20)

if(MSVC)
    target_compile_options(bndl_util PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

add_custom_command(TARGET bndl_util POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libbndl> $<TARGET_FILE_DIR:bndl_util>)

if(LIBBNDL_INSTALL_TOOLS)
    install(TARGETS bndl_util RUNTIME DESTINATION bin
                              LIBRARY DESTINATION lib
                              ARCHIVE DESTINATION lib)
endif()

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

target_link_libraries(bndl_util PRIVATE libtub cxxopts::cxxopts)
if(TARGET pugixml::pugixml)
    target_link_libraries(bndl_util PRIVATE pugixml::pugixml)
else()
    target_include_directories(bndl_util SYSTEM PRIVATE ${pugixml_SOURCE_DIR}/src)
    target_compile_definitions(bndl_util PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
endif()

target_compile_features(bndl_util PRIVATE cxx_std_20)
if(LIBTUB_BUILD_PROJECT_SUPPORT)
    target_compile_definitions(bndl_util PRIVATE LIBTUB_HAS_PROJECT_SUPPORT)
endif()

if(MSVC)
    target_compile_options(bndl_util PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

if(WIN32 AND BUILD_SHARED_LIBS)
    set(BNDL_UTIL_RUNTIME_DLLS $<TARGET_RUNTIME_DLLS:bndl_util>)
    if(TARGET yaml-cpp)
        list(APPEND BNDL_UTIL_RUNTIME_DLLS $<TARGET_FILE:yaml-cpp>)
    elseif(TARGET yaml-cpp::yaml-cpp)
        list(APPEND BNDL_UTIL_RUNTIME_DLLS $<TARGET_FILE:yaml-cpp::yaml-cpp>)
    endif()

    add_custom_command(TARGET bndl_util POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BNDL_UTIL_RUNTIME_DLLS} $<TARGET_FILE_DIR:bndl_util>
        COMMAND_EXPAND_LISTS
    )
endif()

if(LIBTUB_INSTALL_TOOLS)
    install(TARGETS bndl_util)
endif()

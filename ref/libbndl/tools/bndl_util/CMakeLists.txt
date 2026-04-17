add_executable(bndl_util main.cpp)

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts
    GIT_TAG        44380e5a44706ab7347f400698c703eb2a196202 # v3.3.1
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(cxxopts)

target_link_libraries(bndl_util PRIVATE libbndl cxxopts::cxxopts)

set_property(TARGET bndl_util PROPERTY CXX_STANDARD 17)

add_custom_command(TARGET bndl_util POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libbndl> $<TARGET_FILE_DIR:bndl_util>)

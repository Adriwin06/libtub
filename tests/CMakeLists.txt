add_executable(libtub_tests main.cpp)

target_link_libraries(libtub_tests PRIVATE libtub)
target_compile_features(libtub_tests PRIVATE cxx_std_20)
if(NOT LIBTUB_BUILD_PROJECT_SUPPORT)
    target_compile_definitions(libtub_tests PRIVATE LIBTUB_SKIP_PROJECT_TESTS)
endif()

if(MSVC)
    target_compile_options(libtub_tests PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

libtub_copy_runtime_dlls(libtub_tests)

add_test(NAME libtub.tests COMMAND libtub_tests)

set(LIBTUB_TEST_GAME_PATHS "" CACHE STRING "Pipe-separated game roots for the recursive libtub game scan test.")
set(LIBTUB_EXTRACTOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libbndl-extractor")

# The submodule tracks the extractor's "libtub" branch, which builds volatility_libtub_extract against
# this tree. Its "main" branch is a standalone libbndl tool that forces BUILD_SHARED_LIBS=OFF into the
# cache and fails to configure here, so refuse to add that one.
set(LIBTUB_EXTRACTOR_AVAILABLE OFF)
if(EXISTS "${LIBTUB_EXTRACTOR_DIR}/CMakeLists.txt")
    file(READ "${LIBTUB_EXTRACTOR_DIR}/CMakeLists.txt" LIBTUB_EXTRACTOR_CMAKE)
    string(FIND "${LIBTUB_EXTRACTOR_CMAKE}" "volatility_libtub_extract" LIBTUB_EXTRACTOR_TARGET_INDEX)
    if(LIBTUB_EXTRACTOR_TARGET_INDEX GREATER_EQUAL 0)
        set(LIBTUB_EXTRACTOR_AVAILABLE ON)
        # Build it by default when the game scan test needs it. Multi-config generators leave EXCLUDE_FROM_ALL
        # targets out of the top-level solution.
        if(LIBTUB_TEST_GAME_PATHS)
            add_subdirectory(third_party/libbndl-extractor)
        else()
            add_subdirectory(third_party/libbndl-extractor EXCLUDE_FROM_ALL)
        endif()
    else()
        message(WARNING "tests/third_party/libbndl-extractor is not on its libtub branch; skipping volatility_libtub_extract. Run `git submodule update --init tests/third_party/libbndl-extractor`.")
    endif()
endif()

if(LIBTUB_TEST_GAME_PATHS)
    if(NOT LIBTUB_EXTRACTOR_AVAILABLE)
        message(FATAL_ERROR "LIBTUB_TEST_GAME_PATHS was provided but the libtub branch of tests/third_party/libbndl-extractor is not checked out.")
    endif()

    set(LIBTUB_GAME_SCAN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/game-scan")
    add_test(
        NAME libtub.game_scan
        COMMAND volatility_libtub_extract
            --games "${LIBTUB_TEST_GAME_PATHS}"
            --output "${LIBTUB_GAME_SCAN_OUTPUT_DIR}"
            --failures "${LIBTUB_GAME_SCAN_OUTPUT_DIR}/failures.tsv"
    )

    # The extractor only copies the DLLs CMake can see; libtub links yaml-cpp by file, so put it on PATH.
    if(WIN32 AND BUILD_SHARED_LIBS)
        set(LIBTUB_GAME_SCAN_PATH "PATH=path_list_prepend:$<TARGET_FILE_DIR:libtub>")
        if(TARGET yaml-cpp)
            list(APPEND LIBTUB_GAME_SCAN_PATH "PATH=path_list_prepend:$<TARGET_FILE_DIR:yaml-cpp>")
        elseif(TARGET yaml-cpp::yaml-cpp)
            list(APPEND LIBTUB_GAME_SCAN_PATH "PATH=path_list_prepend:$<TARGET_FILE_DIR:yaml-cpp::yaml-cpp>")
        endif()
        set_tests_properties(libtub.game_scan PROPERTIES ENVIRONMENT_MODIFICATION "${LIBTUB_GAME_SCAN_PATH}")
    endif()
endif()

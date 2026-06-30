add_executable(libtub_tests main.cpp)

target_link_libraries(libtub_tests PRIVATE libtub)
target_compile_features(libtub_tests PRIVATE cxx_std_20)
if(NOT LIBTUB_BUILD_PROJECT_SUPPORT)
    target_compile_definitions(libtub_tests PRIVATE LIBTUB_SKIP_PROJECT_TESTS)
endif()

if(MSVC)
    target_compile_options(libtub_tests PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

if(WIN32)
    add_custom_command(TARGET libtub_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:libtub_tests> $<TARGET_FILE_DIR:libtub_tests>
        COMMAND_EXPAND_LISTS
    )
endif()

add_test(NAME libtub.tests COMMAND libtub_tests)

set(LIBTUB_TEST_GAME_PATHS "" CACHE STRING "Pipe-separated game roots for the recursive libtub game scan test.")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libbndl-extractor/third_party/libbndl/CMakeLists.txt")
    add_subdirectory(third_party/libbndl-extractor EXCLUDE_FROM_ALL)

    if(LIBTUB_TEST_GAME_PATHS)
        set(LIBTUB_GAME_SCAN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/game-scan")
        add_test(
            NAME libtub.game_scan
            COMMAND volatility_libtub_extract
                --games "${LIBTUB_TEST_GAME_PATHS}"
                --output "${LIBTUB_GAME_SCAN_OUTPUT_DIR}"
                --failures "${LIBTUB_GAME_SCAN_OUTPUT_DIR}/failures.tsv"
        )
    endif()
elseif(LIBTUB_TEST_GAME_PATHS)
    message(FATAL_ERROR "LIBTUB_TEST_GAME_PATHS was provided but tests/third_party/libbndl-extractor is not checked out.")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libbndl-extractor/CMakeLists.txt")
    message(STATUS "Skipping libbndl-extractor test helper because its nested libbndl submodule is missing.")
endif()

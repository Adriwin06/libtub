add_executable(libtub_tests main.cpp)

target_link_libraries(libtub_tests PRIVATE libtub)
target_compile_features(libtub_tests PRIVATE cxx_std_20)

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

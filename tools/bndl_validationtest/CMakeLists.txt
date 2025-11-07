add_executable(bndl_validationtest main.cpp)

FetchContent_GetProperties(pugixml)

target_link_libraries(bndl_validationtest PRIVATE libbndl cxxopts::cxxopts)
if(TARGET pugixml::pugixml)
    target_link_libraries(bndl_validationtest PRIVATE pugixml::pugixml)
else()
    target_include_directories(bndl_validationtest SYSTEM PRIVATE ${pugixml_SOURCE_DIR}/src)
    target_compile_definitions(bndl_validationtest PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
endif()

set_property(TARGET bndl_validationtest PROPERTY CXX_STANDARD 20)

if(MSVC)
    target_compile_options(bndl_validationtest PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

add_custom_command(TARGET bndl_validationtest POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libbndl> $<TARGET_FILE_DIR:bndl_validationtest>)

if(LIBBNDL_INSTALL_TOOLS)
    install(TARGETS bndl_validationtest RUNTIME DESTINATION bin
                                        LIBRARY DESTINATION lib
                                        ARCHIVE DESTINATION lib)
endif()

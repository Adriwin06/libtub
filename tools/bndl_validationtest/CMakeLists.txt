add_executable(bndl_validationtest main.cpp)

FetchContent_GetProperties(pugixml)

target_link_libraries(bndl_validationtest PRIVATE libtub cxxopts::cxxopts)
if(TARGET pugixml::pugixml)
    target_link_libraries(bndl_validationtest PRIVATE pugixml::pugixml)
else()
    target_include_directories(bndl_validationtest SYSTEM PRIVATE ${pugixml_SOURCE_DIR}/src)
    target_compile_definitions(bndl_validationtest PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
endif()

target_compile_features(bndl_validationtest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(bndl_validationtest PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

if(LIBTUB_INSTALL_TOOLS)
    install(TARGETS bndl_validationtest)
endif()

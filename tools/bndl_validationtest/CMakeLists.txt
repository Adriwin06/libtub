add_executable(bndl_validationtest main.cpp)

target_link_libraries(bndl_validationtest PRIVATE libtub)

target_compile_features(bndl_validationtest PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(bndl_validationtest PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

libtub_copy_runtime_dlls(bndl_validationtest)

if(LIBTUB_INSTALL_TOOLS)
    install(TARGETS bndl_validationtest)
endif()

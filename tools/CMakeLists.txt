option(LIBTUB_INSTALL_TOOLS "Install tools" ON)

add_subdirectory(bndl_util)

option(LIBTUB_BUILD_VALIDATIONTEST "Build validation test tool" OFF)
if(LIBTUB_BUILD_VALIDATIONTEST)
    add_subdirectory(bndl_validationtest)
endif()

add_subdirectory(bndl_util)

option(LIBBNDL_BUILD_UI "Build UI tools" OFF)
if(LIBBNDL_BUILD_UI)
    add_subdirectory(bndl_edit)
endif()

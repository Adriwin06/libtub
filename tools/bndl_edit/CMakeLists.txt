set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

add_executable(bndl_edit main.cpp editor.cpp editor.hpp)

target_link_libraries(bndl_edit libtub Qt6::Widgets)
target_include_directories(bndl_edit PRIVATE)

target_compile_features(bndl_edit PRIVATE cxx_std_20)

option(LIBTUB_INSTALL_UI "Install UI tools" OFF)
if(LIBTUB_INSTALL_TOOLS AND LIBTUB_INSTALL_UI)
    install(TARGETS bndl_edit)

    if(CMAKE_CXX_LINKER_SUPPORTS_PDB)
        install(FILES $<TARGET_PDB_FILE:bndl_edit> TYPE BIN OPTIONAL)
    endif()

    install(TARGETS bndl_edit RUNTIME_DEPENDENCIES
                              PRE_INCLUDE_REGEXES "qt6"
                              PRE_EXCLUDE_REGEXES ".*"
                              DIRECTORIES $<TARGET_FILE_DIR:Qt6::Core>
                              FRAMEWORK DESTINATION Frameworks)

    if(WIN32)
        install(FILES $<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>
                DESTINATION bin/platforms)
        install(FILES $<TARGET_FILE:Qt6::QModernWindowsStylePlugin>
                DESTINATION bin/styles)
    endif()
endif()

option(LIBBNDL_INSTALL_TOOLS "Install tools" ON)

add_subdirectory(bndl_util)

option(LIBBNDL_BUILD_VALIDATIONTEST "Build validation test tool" OFF)
if(LIBBNDL_BUILD_VALIDATIONTEST)
    add_subdirectory(bndl_validationtest)
endif()

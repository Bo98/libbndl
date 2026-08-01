include(FetchContent)
include(GenerateExportHeader)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(HEADER_DIR ${LIBBNDL_ROOT}/include/libbndl)
file(GLOB_RECURSE PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${HEADER_DIR}/*.hpp"
    "${HEADER_DIR}/*.h"
    "${HEADER_DIR}/*.inc"
)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "*.cpp")

add_library(libbndl ${SRC_FILES})

target_sources(libbndl PUBLIC FILE_SET HEADERS
                              BASE_DIRS "${HEADER_DIR}"
                              FILES "${PUBLIC_HEADERS}")
set_source_files_properties("${HEADER_DIR}/bundle.h" PROPERTIES LANGUAGE C
                                                                HEADER_FILE_ONLY ON)

FetchContent_Declare(
    binaryio
    GIT_REPOSITORY https://github.com/Bo98/binaryio
    GIT_TAG        2f169490c7346007e8d3c101031debab61fa804f
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(
    ZLIB
    GIT_REPOSITORY https://github.com/madler/zlib
    GIT_TAG        5a82f71ed1dfc0bec044d9702463dbdf84ea3b71
    SYSTEM
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS
)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml
    GIT_TAG        ee86beb30e4973f5feffe3ce63bfa4fbadf72f38 # v1.15
    SOURCE_SUBDIR  dontbuild
    EXCLUDE_FROM_ALL
    FIND_PACKAGE_ARGS 1.11
)

if(NOT DEFINED ZLIB_BUILD_SHARED)
    set(ZLIB_BUILD_SHARED ${BUILD_SHARED_LIBS})
endif()
set(ZLIB_BUILD_TESTING OFF)
FetchContent_MakeAvailable(binaryio ZLIB pugixml)
if(NOT ZLIB_FOUND AND NOT BUILD_SHARED_LIBS)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

target_link_libraries(libbndl PRIVATE binaryio ZLIB::ZLIB)
if(TARGET pugixml::pugixml)
    target_link_libraries(libbndl PRIVATE pugixml::pugixml)
else()
    target_include_directories(libbndl SYSTEM PRIVATE "${pugixml_SOURCE_DIR}/src")
    target_compile_definitions(libbndl PRIVATE PUGIXML_HEADER_ONLY PUGIXML_NO_XPATH)
endif()

target_compile_features(libbndl PUBLIC c_std_11 cxx_std_20)

set_target_properties(libbndl PROPERTIES PREFIX ""
                                         CXX_VISIBILITY_PRESET hidden
                                         POSITION_INDEPENDENT_CODE ON
                                         VERIFY_INTERFACE_HEADER_SETS ${PROJECT_IS_TOP_LEVEL})

generate_export_header(libbndl EXPORT_FILE_NAME include/libbndl/internal/export.h)

target_include_directories(libbndl PUBLIC "${LIBBNDL_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/include")

if(MSVC)
    target_compile_options(libbndl PRIVATE "/W4" "/Zc:throwingNew" "/EHsc")
endif()

install(TARGETS libbndl FILE_SET HEADERS)
if(CMAKE_CXX_LINKER_SUPPORTS_PDB AND BUILD_SHARED_LIBS)
    install(FILES $<TARGET_PDB_FILE:libbndl> TYPE BIN OPTIONAL)
endif()

include(GNUInstallDirs)
install(FILES "${LIBBNDL_ROOT}/LICENSE.md" TYPE DOC RENAME copyright)

if(NOT ZLIB_FOUND AND ZLIB_BUILD_SHARED)
    install(TARGETS zlib ARCHIVE EXCLUDE_FROM_ALL)
endif()

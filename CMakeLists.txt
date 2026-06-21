cmake_minimum_required(VERSION 3.28)

project(
    qtcode
    VERSION 0.1.0
    DESCRIPTION "Native KDE/Linux developer cockpit for AI agents and terminals"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(QtCodeCompilerOptions)
include(QtCodeHelpers)
include(QtCodeDependencies)

add_subdirectory(src)

option(QTcode_BUILD_TESTS "Build QTCode tests" OFF)
if(QTcode_BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

install(
    DIRECTORY resources/workspace-bundle/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/qtcode/workspace-bundle
)
install(
    DIRECTORY scripts/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/qtcode/workspace-bundle/scripts
    USE_SOURCE_PERMISSIONS
)
install(
    DIRECTORY tools/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/qtcode/workspace-bundle/tools
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING PATTERN "*.py"
)

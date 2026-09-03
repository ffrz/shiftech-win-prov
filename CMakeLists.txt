cmake_minimum_required(VERSION 3.24)
project(ShiftechWinProvisioner)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SHIFTECH_BUILD_GUI "Build the Qt Widgets front-end" OFF)
option(SHIFTECH_BUILD_TESTS "Build the QtTest suite" ON)
option(SHIFTECH_INTEGRATION_TESTS "Enable tests that call pnputil / winget" OFF)
option(SHIFTECH_WARNINGS_AS_ERRORS "-Werror" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network)
if(SHIFTECH_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()
if(SHIFTECH_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
endif()

qt_standard_project_setup()

include(cmake/CompilerWarnings.cmake)

add_subdirectory(src/core)
add_subdirectory(src/cli)

if(SHIFTECH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

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

# --- app icon: compile favicon.ico into a .rc that each exe links ---
set(APP_ICON_ICO "${CMAKE_SOURCE_DIR}/favicon.ico")
if(WIN32 AND EXISTS "${APP_ICON_ICO}")
    set(APP_ICON_PATH "${APP_ICON_ICO}")
    configure_file("${CMAKE_SOURCE_DIR}/cmake/app.rc.in"
                   "${CMAKE_BINARY_DIR}/app_icon.rc" @ONLY)
    set(SHIFTECH_APP_ICON_RC "${CMAKE_BINARY_DIR}/app_icon.rc"
        CACHE INTERNAL "generated .rc with the app icon")
else()
    set(SHIFTECH_APP_ICON_RC "" CACHE INTERNAL "")
endif()

add_subdirectory(src/core)
add_subdirectory(src/cli)

if(SHIFTECH_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

if(SHIFTECH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

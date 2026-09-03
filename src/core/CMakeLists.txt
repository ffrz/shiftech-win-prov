add_library(shiftech_core STATIC
    system/SystemInspector.cpp
    hardware/Device.cpp
    hardware/HardwareId.cpp
    hardware/DeviceEnumerator.cpp
    drivers/TargetSystem.cpp
    drivers/DriverMatch.cpp
    drivers/MockDriverProvider.cpp
    profiles/ProfileLoader.cpp
    applications/WinGetOutput.cpp
    applications/WinGetProvider.cpp
)

target_include_directories(shiftech_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(shiftech_core PUBLIC Qt6::Core Qt6::Network setupapi cfgmgr32 advapi32 version)

set_shiftech_warnings(shiftech_core)

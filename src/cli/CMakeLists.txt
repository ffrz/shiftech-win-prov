qt_add_executable(provisioner
    main.cpp
    commands/ScanCommand.cpp
    commands/DriversScanCommand.cpp
    commands/DriversResolveCommand.cpp
    commands/AppsInstallCommand.cpp
)

set_target_properties(provisioner PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(provisioner PRIVATE shiftech_core Qt6::Core)

set_shiftech_warnings(provisioner)

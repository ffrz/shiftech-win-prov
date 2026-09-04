qt_add_executable(provisioner
    main.cpp
    commands/ScanCommand.cpp
    commands/DriversScanCommand.cpp
    commands/DriversResolveCommand.cpp
    commands/DriversInstallCommand.cpp
    commands/AppsInstallCommand.cpp
    commands/ProvisionCommand.cpp
    commands/ResetCommand.cpp
    ${SHIFTECH_APP_ICON_RC}
)

set_target_properties(provisioner PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(provisioner PRIVATE shiftech_core Qt6::Core)

set_shiftech_warnings(provisioner)

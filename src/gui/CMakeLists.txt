# GUI widgets as a static lib so both the app and the smoke test can link them.
qt_add_library(shiftech_gui_lib STATIC
    EngineController.cpp
    ChecklistTabs.cpp
    MainWindow.cpp
    resources.qrc
)
target_link_libraries(shiftech_gui_lib PUBLIC shiftech_core Qt6::Core Qt6::Widgets)
target_include_directories(shiftech_gui_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_shiftech_warnings(shiftech_gui_lib)

qt_add_executable(shiftech_gui WIN32 main.cpp ${SHIFTECH_APP_ICON_RC})
target_link_libraries(shiftech_gui PRIVATE shiftech_gui_lib)
set_shiftech_warnings(shiftech_gui)

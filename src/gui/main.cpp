#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Shiftech Win Provisioner");
    app.setOrganizationName("Shiftech");

    shiftech::gui::MainWindow window;
    window.show();
    return app.exec();
}

#include "MainWindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Shiftech Win Provisioner");
    app.setOrganizationName("Shiftech");
    app.setWindowIcon(QIcon(":/app.ico"));

    shiftech::gui::MainWindow window;
    window.show();
    return app.exec();
}

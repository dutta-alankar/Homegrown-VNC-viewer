#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char* argv[]) {
    // Avoid GPU driver issues (for example zink/Vulkan init failures) that can
    // prevent the viewer from rendering despite a successful VNC connection.
    QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);

    QApplication app(argc, argv);
    QApplication::setApplicationName("vnc-client");
    QApplication::setOrganizationName("local");

    MainWindow window;
    window.show();

    return app.exec();
}

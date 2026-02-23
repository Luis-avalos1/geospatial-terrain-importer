#include <QApplication>
#include <QSurfaceFormat>

#include "gui/MainWindow.hpp"

int main(int argc, char *argv[])
{
    // OpenGL 4.1 Core is the highest Apple supports; request it explicitly so
    // Qt doesn't accidentally fall back to a compatibility profile.
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);  // MSAA
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("Terrain Importer");
    app.setOrganizationName("geospatial-tools");
    app.setApplicationVersion("0.1.0");

    MainWindow win;
    win.show();

    return app.exec();
}

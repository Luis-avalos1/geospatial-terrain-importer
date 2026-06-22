#include <QApplication>
#include <QSurfaceFormat>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QFile>
#include <QTimer>
#include <QPixmap>

#include <cstdlib>

#include "gui/MainWindow.hpp"

// A cohesive dark palette so the bits QSS doesn't reach (title bar chrome,
// disabled states) match the stylesheet.
static void applyDarkPalette(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    const QColor bg(0x0d, 0x11, 0x17), base(0x0e, 0x13, 0x19), panel(0x11, 0x16, 0x1d);
    const QColor text(0xe6, 0xed, 0xf3), muted(0x8b, 0x98, 0xa5), accent(0xe0, 0xa1, 0x3a);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, panel);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, panel);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::ToolTipBase, panel);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, QColor(0x1a, 0x12, 0x06));
    p.setColor(QPalette::PlaceholderText, muted);
    p.setColor(QPalette::Disabled, QPalette::Text, muted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    app.setPalette(p);
}

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

    applyDarkPalette(app);

    QFile qss(":/styles/style.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    MainWindow win;
    win.show();

    // ── Dev hooks (no effect unless the env vars are set) ──────────────────────
    // TERRAIN_OPEN=<path>   auto-import a DEM on launch
    // TERRAIN_SHOT=<path>   grab the window to a PNG and quit (UI verification)
    if (const char *open = std::getenv("TERRAIN_OPEN"); open && *open)
        win.openPath(QString::fromUtf8(open));

    if (const char *shot = std::getenv("TERRAIN_SHOT"); shot && *shot) {
        const QString shotPath = QString::fromUtf8(shot);
        QTimer::singleShot(3500, [&win, shotPath]() {
            win.grab().save(shotPath);
            QApplication::quit();
        });
    }

    // TERRAIN_DEMO=<outdir>  +  TERRAIN_DEMO_DEM=<dem>  record the demo-video frames
    if (const char *demoOut = std::getenv("TERRAIN_DEMO"); demoOut && *demoOut) {
        const QString outDir = QString::fromUtf8(demoOut);
        const char *demEnv = std::getenv("TERRAIN_DEMO_DEM");
        const QString dem = demEnv && *demEnv ? QString::fromUtf8(demEnv) : QString();
        // Let the window map and the file model start populating first.
        QTimer::singleShot(500, [&win, dem, outDir]() {
            win.runCaptureDemo(dem, outDir);
        });
    }

    return app.exec();
}

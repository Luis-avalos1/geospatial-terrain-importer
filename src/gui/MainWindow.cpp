#include "MainWindow.hpp"

#include "FileBrowserPanel.hpp"
#include "ImportControlsWidget.hpp"
#include "SettingsPanel.hpp"
#include "ProgressDialog.hpp"
#include "renderer/TerrainRenderer.hpp"
#include "core/GeoTiffReader.hpp"
#include "core/SatelliteImageLoader.hpp"
#include "core/LodManager.hpp"
#include "core/TextureAtlas.hpp"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QSplitter>
#include <QDockWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include <QCloseEvent>
#include <QTimer>
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QPixmap>
#include <QLinearGradient>
#include <QFont>
#include <QFontMetrics>
#include <QPen>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <stdexcept>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Terrain Importer");
    resize(1280, 800);

    // ── Central area: left splitter | renderer ────────────────────────────────
    auto *rootSplit = new QSplitter(Qt::Horizontal, this);

    // Left panel: file browser on top, import controls on bottom
    auto *leftSplit = new QSplitter(Qt::Vertical, rootSplit);
    m_fileBrowser = new FileBrowserPanel(leftSplit);
    m_importCtrl  = new ImportControlsWidget(leftSplit);
    leftSplit->addWidget(m_fileBrowser);
    leftSplit->addWidget(m_importCtrl);
    leftSplit->setStretchFactor(0, 3);
    leftSplit->setStretchFactor(1, 1);
    leftSplit->setMaximumWidth(280);

    m_renderer = new TerrainRenderer(rootSplit);
    rootSplit->addWidget(leftSplit);
    rootSplit->addWidget(m_renderer);
    rootSplit->setStretchFactor(0, 0);
    rootSplit->setStretchFactor(1, 1);

    setCentralWidget(rootSplit);

    // ── Right dock: settings ──────────────────────────────────────────────────
    m_settings = new SettingsPanel(this);
    m_settings->setRenderer(m_renderer);

    auto *dock = new QDockWidget("Settings", this);
    dock->setObjectName("SettingsDock");   // needed by saveState()
    dock->setWidget(m_settings);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // ── Progress dialog ───────────────────────────────────────────────────────
    m_progress = new ProgressDialog(this);
    m_progress->setCancellable(false);

    setupMenuBar();
    setupStatusBar();

    // ── Wiring ───────────────────────────────────────────────────────────────
    connect(m_fileBrowser, &FileBrowserPanel::fileActivated,
            this,          &MainWindow::onFileActivated);

    connect(m_importCtrl, &ImportControlsWidget::importRequested,
            this,         &MainWindow::onImportRequested);

    connect(m_renderer, &TerrainRenderer::fpsUpdated,
            this,       &MainWindow::onFpsUpdated);

    connect(&m_watcher, &QFutureWatcher<LoadResult>::finished,
            this,       &MainWindow::onLoadFinished);

    // Optional: start the file browser at a specific folder (dev/screenshots).
    if (const char *root = std::getenv("TERRAIN_ROOT"); root && *root)
        m_fileBrowser->setRootPath(QString::fromUtf8(root));

    // Restore window geometry
    QSettings s;
    restoreGeometry(s.value("window/geometry").toByteArray());
    restoreState(s.value("window/state").toByteArray());
}

MainWindow::~MainWindow() = default;

void MainWindow::openPath(const QString &path)
{
    if (!path.isEmpty()) loadFile(path);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    m_settings->saveSettings();
    QSettings s;
    s.setValue("window/geometry", saveGeometry());
    s.setValue("window/state",    saveState());
    QMainWindow::closeEvent(e);
}

// ── Menu / status bar ─────────────────────────────────────────────────────────

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");

    auto *openAct = fileMenu->addAction("&Open GeoTIFF…", this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Open GeoTIFF", QDir::homePath(),
            "Raster files (*.tif *.tiff *.img *.png *.jpg *.jpeg);;All files (*)"
        );
        if (!path.isEmpty()) loadFile(path);
    }, QKeySequence::Open);
    Q_UNUSED(openAct);

    fileMenu->addSeparator();
    fileMenu->addAction("&Quit", qApp, &QApplication::quit, QKeySequence::Quit);

    auto *viewMenu = menuBar()->addMenu("&View");
    auto *dockToggle = viewMenu->addAction("Settings panel");
    dockToggle->setCheckable(true);
    dockToggle->setChecked(true);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_fpsLabel = new QLabel("-- fps", this);
    statusBar()->addPermanentWidget(m_fpsLabel);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onFileActivated(const QString &path)
{
    m_pendingPath = path;
    m_statusLabel->setText(path);
}

void MainWindow::onImportRequested()
{
    if (m_pendingPath.isEmpty()) {
        QMessageBox::information(this, "No file selected",
            "Please select a GeoTIFF or raster file in the file browser first.");
        return;
    }
    loadFile(m_pendingPath);
}

void MainWindow::loadFile(const QString &path)
{
    if (m_watcher.isRunning()) return;  // already loading

    const ImportOptions opts = m_importCtrl->options();

    m_progress->setMessage("Loading " + path + "…");
    m_progress->setProgress(-1);
    m_progress->show();

    const std::string stdPath = path.toStdString();

    QFuture<LoadResult> future = QtConcurrent::run([stdPath, opts]() -> LoadResult {
        LoadResult result;
        try {
            ElevationData elev = GeoTiffReader::read(
                stdPath,
                opts.resolution,
                opts.resolution
            );

            auto lodMgr = std::make_shared<LodManager>();
            LodManager::Options lodOpts;
            lodOpts.levels        = opts.lodLevels;
            lodOpts.heightScale   = opts.heightScale;
            lodOpts.runSimplifier = opts.runSimplifier;
            lodOpts.simplifyRatio = opts.simplifyRatio;
            lodMgr->build(elev, lodOpts);

            if (opts.buildAtlas) {
                // NOTE: this re-opens/decodes the raster (GeoTiffReader::read
                // already opened it above). Acceptable for now — atlas building
                // is opt-in and one-time; sharing the GDAL dataset between the
                // elevation and image loaders would avoid the second decode.
                ImageTile img = SatelliteImageLoader::load(stdPath);
                auto atlas = std::make_shared<TextureAtlas>();
                std::vector<const ImageTile *> tiles = {&img};
                atlas->pack(tiles, 4096);
                result.atlas = atlas;
            }

            result.lodMgr = lodMgr;
        } catch (const std::exception &ex) {
            result.error = QString::fromUtf8(ex.what());
        }
        return result;
    });

    m_watcher.setFuture(future);
}

void MainWindow::onLoadFinished()
{
    m_progress->hide();

    const LoadResult res = m_watcher.result();

    if (!res.error.isEmpty()) {
        QMessageBox::critical(this, "Import failed", res.error);
        m_statusLabel->setText("Import failed");
        return;
    }

    // Stash the shared_ptrs so the data outlives the renderer's use of the raw ptr
    m_activeLodMgr = res.lodMgr;
    m_activeAtlas  = res.atlas;

    m_renderer->clearTerrain();
    m_renderer->loadMesh(m_activeLodMgr.get());

    if (m_activeAtlas && !m_activeAtlas->empty())
        m_renderer->loadAtlasTexture(*m_activeAtlas);

    m_statusLabel->setText(QString("Loaded — %1 LOD levels")
        .arg(m_activeLodMgr ? m_activeLodMgr->levels().size() : 0));
}

void MainWindow::onFpsUpdated(double fps)
{
    m_fpsLabel->setText(QString("%1 fps").arg(fps, 0, 'f', 1));
}

// ── Demo-capture mode ───────────────────────────────────────────────────────────
//
// Scripts the whole tour and grab()s one composited frame per tick (captures the
// GL viewport too); ffmpeg assembles the frames into the walkthrough video.

namespace {

const QColor kAccent(0xE0, 0xA1, 0x3A);   // amber, matches the app theme
const QColor kInk   (0xE6, 0xED, 0xF3);
const QColor kMuted (0x9F, 0xAE, 0xBC);

double smoothstep(double a, double b, double t)
{
    if (b <= a) return t < a ? 0.0 : 1.0;
    t = std::clamp((t - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

QColor withAlpha(const QColor &c, double a)
{
    return QColor(c.red(), c.green(), c.blue(), int(std::clamp(a, 0.0, 1.0) * 255));
}

void drawProgressLine(QPainter &p, const QSize &sz, double frac)
{
    p.fillRect(0, 0, sz.width(), 4, QColor(255, 255, 255, 26));
    p.fillRect(0, 0, int(sz.width() * std::clamp(frac, 0.0, 1.0)), 4, kAccent);
}

void drawLowerThird(QPainter &p, const QSize &sz, const QString &eyebrow,
                    const QString &title, const QString &subtitle)
{
    const int W = sz.width(), H = sz.height();
    QLinearGradient g(0, H - 168, 0, H);
    g.setColorAt(0.0,  QColor(8, 11, 16, 0));
    g.setColorAt(0.35, QColor(8, 11, 16, 195));
    g.setColorAt(1.0,  QColor(8, 11, 16, 240));
    p.fillRect(0, H - 168, W, 168, g);

    const int xTab = 64;
    p.fillRect(xTab, H - 132, 5, 92, kAccent);
    const int tx = xTab + 22;

    QFont fe("Helvetica Neue"); fe.setPixelSize(19); fe.setBold(true);
    fe.setLetterSpacing(QFont::AbsoluteSpacing, 2.6);
    p.setFont(fe); p.setPen(kAccent);
    p.drawText(tx, H - 118, eyebrow.toUpper());

    QFont ft("Helvetica Neue"); ft.setPixelSize(40); ft.setBold(true);
    p.setFont(ft); p.setPen(kInk);
    p.drawText(tx, H - 74, title);

    QFont fs("Helvetica Neue"); fs.setPixelSize(23);
    p.setFont(fs); p.setPen(kMuted);
    p.drawText(tx, H - 40, subtitle);
}

void drawTitleCard(QPainter &p, const QSize &sz, const QString &title,
                   const QString &subtitle, double alpha)
{
    if (alpha <= 0.0) return;
    const int W = sz.width(), H = sz.height();
    p.fillRect(QRect(0, 0, W, H), QColor(6, 9, 13, int(150 * alpha)));

    const int cx = W / 2, cy = H / 2;
    p.fillRect(cx - 44, cy - 86, 88, 4, withAlpha(kAccent, alpha));

    QFont ft("Helvetica Neue"); ft.setPixelSize(62); ft.setBold(true);
    ft.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(ft); p.setPen(withAlpha(kInk, alpha));
    p.drawText(QRect(0, cy - 56, W, 80), Qt::AlignHCenter | Qt::AlignTop, title);

    QFont fs("Helvetica Neue"); fs.setPixelSize(26);
    fs.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
    p.setFont(fs); p.setPen(withAlpha(kMuted, alpha));
    p.drawText(QRect(0, cy + 30, W, 40), Qt::AlignHCenter | Qt::AlignTop, subtitle);
}

void drawProcessing(QPainter &p, const QSize &sz, double t, const QString &label)
{
    const int W = sz.width(), H = sz.height();
    const int barW = 440, barH = 8;
    const int x = W / 2 - barW / 2, y = H / 2 + 24;

    QFont fl("Helvetica Neue"); fl.setPixelSize(22); fl.setBold(true);
    p.setFont(fl); p.setPen(kInk);
    p.drawText(QRect(0, y - 52, W, 28), Qt::AlignHCenter | Qt::AlignTop, label);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 28));
    p.drawRoundedRect(x, y, barW, barH, 4, 4);

    const int chunk = 150;
    const double pos = t - std::floor(t);
    p.save();
    p.setClipRect(x, y, barW, barH);
    p.setBrush(kAccent);
    p.drawRoundedRect(x + int((barW + chunk) * pos) - chunk, y, chunk, barH, 4, 4);
    p.restore();
}

}  // namespace

void MainWindow::runCaptureDemo(const QString &demPath, const QString &outDir)
{
    m_demoOutDir = outDir;
    QDir().mkpath(outDir);

    // 16:9 client area so the recording crops cleanly to 1080p.
    resize(1280, 720);

    // List the DEM's folder so the browser shows realistic neighbours, and give
    // QFileSystemModel a head start at populating before we select a row.
    m_fileBrowser->setRootPath(QFileInfo(demPath).absolutePath());
    m_pendingPath = demPath;

    // Clean render defaults.
    m_settings->demoSetWireframe(false);
    m_settings->demoSetNormals(false);
    m_settings->demoSetExaggeration(1.0);

    // Build the terrain synchronously up front (avoids a mid-capture stall). It is
    // held in m_activeLodMgr but not handed to the renderer until the reveal beat.
    try {
        const ImportOptions opts = m_importCtrl->options();
        ElevationData elev = GeoTiffReader::read(
            demPath.toStdString(), opts.resolution, opts.resolution);

        auto lodMgr = std::make_shared<LodManager>();
        LodManager::Options lodOpts;
        lodOpts.levels        = opts.lodLevels;
        lodOpts.heightScale   = opts.heightScale;
        lodOpts.runSimplifier = opts.runSimplifier;
        lodOpts.simplifyRatio = opts.simplifyRatio;
        lodMgr->build(elev, lodOpts);
        m_activeLodMgr = lodMgr;
    } catch (const std::exception &ex) {
        qWarning("demo: failed to load %s: %s", qPrintable(demPath), ex.what());
        QApplication::quit();
        return;
    }

    m_renderer->clearTerrain();   // viewport starts empty for the intro beats

    m_demoFrame = 0;
    m_demoTimer = new QTimer(this);
    connect(m_demoTimer, &QTimer::timeout, this, &MainWindow::demoTick);
    m_demoTimer->start(8);
}

void MainWindow::demoTick()
{
    // ── Storyboard (frame counts @ 30 fps) ──────────────────────────────────────
    enum { TITLE, BROWSE, IMPORT, REVEAL, ORBIT, EXAGG, WIRE, NORM, LOD, OUTRO, N };
    static const int len[N]   = { 75, 240, 120, 90, 210, 240, 200, 180, 210, 195 };
    static int start[N] = {0};
    static int total = 0;
    if (total == 0) { int a = 0; for (int i = 0; i < N; ++i) { start[i] = a; a += len[i]; } total = a; }

    const int f = m_demoFrame;
    int scene = N - 1;
    for (int i = 0; i < N; ++i) { if (f < start[i] + len[i]) { scene = i; break; } }
    const int    lf = f - start[scene];
    const double lt = len[scene] > 1 ? double(lf) / (len[scene] - 1) : 0.0;

    QString eyebrow, title, subtitle;
    bool   titleCard = false, processing = false;
    double cardAlpha = 1.0, fadeBlack = 0.0;
    const QString demName = QFileInfo(m_pendingPath).fileName();

    switch (scene) {
    case TITLE:
        titleCard = true;
        title = "GEOSPATIAL TERRAIN IMPORTER";
        subtitle = "Real-world elevation  →  interactive 3D terrain";
        cardAlpha = smoothstep(0.0, 0.22, lt) * (1.0 - smoothstep(0.88, 1.0, lt));
        fadeBlack = 1.0 - smoothstep(0.0, 0.18, lt);
        break;

    case BROWSE:
        if (lf == 24 || lf == 90 || lf == 160) m_fileBrowser->demoSelect(m_pendingPath);
        eyebrow  = "Step 1 · Load";
        title    = "Open a GeoTIFF DEM";
        subtitle = "GDAL ingests any elevation raster — pick a file in the browser";
        break;

    case IMPORT:
        processing = true;
        eyebrow  = "Step 2 · Import";
        title    = "Building the terrain";
        subtitle = "Resample · triangulate height-field · build the LOD pyramid";
        break;

    case REVEAL:
        if (lf == 0) m_renderer->loadMesh(m_activeLodMgr.get());  // loadMesh reframes the camera on the new bounds
        else         m_renderer->demoOrbit(0.5f, 0.0f);
        eyebrow  = "Rendered";
        title    = QString("Imported — %1 LOD levels")
                     .arg(m_activeLodMgr ? int(m_activeLodMgr->levels().size()) : 0);
        subtitle = "Per-vertex normals · hypsometric colormap + hillshade";
        fadeBlack = (1.0 - smoothstep(0.0, 0.28, lt)) * 0.9;
        break;

    case ORBIT:
        m_renderer->demoOrbit(1.1f, 0.0f);
        eyebrow  = "Navigate";
        title    = "Orbit & explore";
        subtitle = "Arcball camera — drag to orbit, scroll to zoom";
        break;

    case EXAGG: {
        m_renderer->demoOrbit(0.5f, 0.0f);
        const double amp = smoothstep(0.0, 0.45, lt) - smoothstep(0.55, 1.0, lt);
        m_settings->demoSetExaggeration(1.0 + 1.4 * amp);   // 1.0× → 2.4× → 1.0×
        eyebrow  = "Render setting";
        title    = "Vertical exaggeration";
        subtitle = "Amplify relief live on the GPU — no re-import";
        break;
    }

    case WIRE:
        if (lf == 0) m_settings->demoSetWireframe(true);
        m_renderer->demoOrbit(1.0f, 0.0f);
        eyebrow  = "Render setting";
        title    = "Wireframe";
        subtitle = "The triangulated height-field mesh";
        break;

    case NORM:
        if (lf == 0) { m_settings->demoSetWireframe(false); m_settings->demoSetNormals(true); }
        m_renderer->demoOrbit(1.0f, 0.0f);
        eyebrow  = "Render setting";
        title    = "Surface normals";
        subtitle = "Per-vertex normals drive the lighting (RGB debug view)";
        break;

    case LOD:
        if (lf == 0) { m_settings->demoSetNormals(false); m_settings->demoSetWireframe(true); }
        m_renderer->demoOrbit(0.45f, 0.0f);
        m_renderer->demoZoom(lf < len[LOD] / 2 ? -0.55f : 0.55f);
        eyebrow  = "Performance";
        title    = "Level of detail";
        subtitle = "Distance-based LOD swaps mesh resolution automatically";
        break;

    case OUTRO:
        if (lf == 0) m_settings->demoSetWireframe(false);
        m_renderer->demoOrbit(0.7f, 0.0f);
        m_settings->demoSetExaggeration(1.0 + 0.35 * smoothstep(0.0, 0.6, lt));
        titleCard = true;
        title     = "GEOSPATIAL TERRAIN IMPORTER";
        subtitle  = "C++ · Qt · OpenGL 4.1   —   github.com/Luis-avalos1/geospatial-terrain-importer";
        cardAlpha = smoothstep(0.12, 0.45, lt);
        fadeBlack = smoothstep(0.78, 1.0, lt);
        break;
    }

    // ── Compose the frame ───────────────────────────────────────────────────────
    m_renderer->repaint();   // synchronous GL render of the current state

    QImage img = grab().toImage().convertToFormat(QImage::Format_RGB32);
    img = img.scaled(1920, 1080, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    img.setDevicePixelRatio(1.0);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    drawProgressLine(p, img.size(), total > 1 ? double(f) / (total - 1) : 0.0);
    if (processing)
        drawProcessing(p, img.size(), lf / 30.0, "Importing  " + demName);
    if (titleCard)
        drawTitleCard(p, img.size(), title, subtitle, cardAlpha);
    else
        drawLowerThird(p, img.size(), eyebrow, title, subtitle);
    if (fadeBlack > 0.0)
        p.fillRect(img.rect(), QColor(0, 0, 0, int(std::clamp(fadeBlack, 0.0, 1.0) * 255)));
    p.end();

    img.save(QString("%1/frame_%2.jpg").arg(m_demoOutDir).arg(f, 5, 10, QChar('0')),
             "JPG", 92);

    if (++m_demoFrame >= total) {
        m_demoTimer->stop();
        QApplication::quit();
    }
}

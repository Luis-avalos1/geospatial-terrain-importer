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

    // Restore window geometry
    QSettings s;
    restoreGeometry(s.value("window/geometry").toByteArray());
    restoreState(s.value("window/state").toByteArray());
}

MainWindow::~MainWindow() = default;

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

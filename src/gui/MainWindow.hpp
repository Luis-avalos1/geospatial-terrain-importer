#pragma once

#include <QMainWindow>
#include <QString>
#include <QFutureWatcher>
#include <memory>

class TerrainRenderer;
class FileBrowserPanel;
class ImportControlsWidget;
class SettingsPanel;
class ProgressDialog;
class LodManager;
class TextureAtlas;
class QLabel;
class QSplitter;
class QDockWidget;
class QTimer;

// Package produced by the background worker
struct LoadResult {
    std::shared_ptr<LodManager>   lodMgr;
    std::shared_ptr<TextureAtlas> atlas;
    QString error;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Import a raster directly (used by the file menu and the launch dev hook).
    void openPath(const QString &path);

    // Records the demo reel: scripts the full upload→render→settings tour and
    // writes one numbered frame per tick to outDir, then quits. The frames are
    // assembled into the demo video with ffmpeg.
    void runCaptureDemo(const QString &demPath, const QString &outDir);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void onFileActivated(const QString &path);
    void onImportRequested();
    void onLoadFinished();
    void onFpsUpdated(double fps);

private:
    void setupMenuBar();
    void setupStatusBar();
    void loadFile(const QString &path);

    // Demo-capture mode helpers (see runCaptureDemo).
    void demoTick();

    QTimer *m_demoTimer = nullptr;
    QString m_demoOutDir;
    int     m_demoFrame = 0;

    TerrainRenderer      *m_renderer    = nullptr;
    FileBrowserPanel     *m_fileBrowser = nullptr;
    ImportControlsWidget *m_importCtrl  = nullptr;
    SettingsPanel        *m_settings    = nullptr;
    ProgressDialog       *m_progress    = nullptr;
    QLabel               *m_fpsLabel   = nullptr;
    QLabel               *m_statusLabel = nullptr;

    QString m_pendingPath;

    // Keep the loaded data alive as long as the renderer is using it
    std::shared_ptr<LodManager>   m_activeLodMgr;
    std::shared_ptr<TextureAtlas> m_activeAtlas;

    QFutureWatcher<LoadResult> m_watcher;
};

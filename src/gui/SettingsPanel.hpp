#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;

class TerrainRenderer;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // Wire up to the renderer after it's created
    void setRenderer(TerrainRenderer *renderer);

    void saveSettings() const;
    void loadSettings();

private slots:
    void onWireframeToggled(bool on);
    void onNormalsToggled(bool on);
    void onAtlasSizeChanged(int idx);
    void onResetCamera();

private:
    TerrainRenderer *m_renderer = nullptr;

    QComboBox *m_atlasSizeCombo = nullptr;
    QComboBox *m_crsCombo       = nullptr;
    QCheckBox *m_wireframeCheck = nullptr;
    QCheckBox *m_normalsCheck   = nullptr;
    QPushButton *m_resetCamBtn  = nullptr;
};

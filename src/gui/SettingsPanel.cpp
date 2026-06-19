#include "SettingsPanel.hpp"
#include "renderer/TerrainRenderer.hpp"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QLabel>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);

    // ── Render options ────────────────────────────────────────────────────────
    auto *renderGrp = new QGroupBox("Render", this);
    auto *renderForm = new QFormLayout(renderGrp);

    m_wireframeCheck = new QCheckBox("Wireframe", this);
    renderForm->addRow(m_wireframeCheck);

    m_normalsCheck = new QCheckBox("Show normals", this);
    renderForm->addRow(m_normalsCheck);

    m_resetCamBtn = new QPushButton("Reset camera", this);
    renderForm->addRow(m_resetCamBtn);

    outer->addWidget(renderGrp);

    // ── CRS ───────────────────────────────────────────────────────────────────
    auto *crsGrp = new QGroupBox("Coordinate Reference", this);
    auto *crsForm = new QFormLayout(crsGrp);

    m_crsCombo = new QComboBox(this);
    m_crsCombo->addItems({"Auto-detect", "EPSG:4326 (WGS84)", "EPSG:3857 (Web Mercator)"});
    crsForm->addRow("CRS:", m_crsCombo);

    outer->addWidget(crsGrp);

    // ── Atlas ─────────────────────────────────────────────────────────────────
    auto *atlasGrp = new QGroupBox("Texture Atlas", this);
    auto *atlasForm = new QFormLayout(atlasGrp);

    m_atlasSizeCombo = new QComboBox(this);
    m_atlasSizeCombo->addItems({"1024", "2048", "4096", "8192"});
    m_atlasSizeCombo->setCurrentText("4096");
    atlasForm->addRow("Atlas size:", m_atlasSizeCombo);

    outer->addWidget(atlasGrp);
    outer->addStretch();

    connect(m_wireframeCheck, &QCheckBox::toggled, this, &SettingsPanel::onWireframeToggled);
    connect(m_normalsCheck,   &QCheckBox::toggled, this, &SettingsPanel::onNormalsToggled);
    connect(m_resetCamBtn,    &QPushButton::clicked, this, &SettingsPanel::onResetCamera);
    connect(m_atlasSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsPanel::onAtlasSizeChanged);

    loadSettings();
}

void SettingsPanel::setRenderer(TerrainRenderer *renderer)
{
    m_renderer = renderer;
    // Sync the current toggle states to the renderer (loadSettings ran before
    // the renderer existed, so the renderer would otherwise miss them).
    if (m_renderer) {
        m_renderer->setWireframe(m_wireframeCheck->isChecked());
        m_renderer->setShowNormals(m_normalsCheck->isChecked());
    }
}

void SettingsPanel::saveSettings() const
{
    QSettings s;
    // Wireframe / normals are transient debug views (see loadSettings) — not persisted.
    s.setValue("atlas/size",        m_atlasSizeCombo->currentText());
    s.setValue("crs/selected",      m_crsCombo->currentIndex());
}

void SettingsPanel::loadSettings()
{
    QSettings s;
    // Wireframe / normals are transient debug views — always start cleared.
    m_wireframeCheck->setChecked(false);
    m_normalsCheck->setChecked(false);
    m_atlasSizeCombo->setCurrentText(s.value("atlas/size", "4096").toString());
    m_crsCombo->setCurrentIndex(s.value("crs/selected", 0).toInt());
}

void SettingsPanel::onWireframeToggled(bool on)
{
    if (m_renderer) m_renderer->setWireframe(on);
    saveSettings();
}

void SettingsPanel::onNormalsToggled(bool on)
{
    if (m_renderer) m_renderer->setShowNormals(on);
    saveSettings();
}

void SettingsPanel::onAtlasSizeChanged(int)
{
    saveSettings();
}

void SettingsPanel::onResetCamera()
{
    if (m_renderer) m_renderer->resetView();
}

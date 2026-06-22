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
#include <QSlider>
#include <QHBoxLayout>
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

    // Live vertical exaggeration. Slider units are hundredths (100 = 1.0×);
    // range 0.5×–3.0×.
    m_exaggSlider = new QSlider(Qt::Horizontal, this);
    m_exaggSlider->setRange(50, 300);
    m_exaggSlider->setSingleStep(5);
    m_exaggSlider->setPageStep(25);
    m_exaggSlider->setValue(100);
    m_exaggValue = new QLabel("1.0×", this);
    m_exaggValue->setMinimumWidth(36);
    m_exaggValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto *exaggRow = new QWidget(this);
    auto *exaggLayout = new QHBoxLayout(exaggRow);
    exaggLayout->setContentsMargins(0, 0, 0, 0);
    exaggLayout->addWidget(m_exaggSlider, 1);
    exaggLayout->addWidget(m_exaggValue);
    renderForm->addRow("Exaggeration:", exaggRow);

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
    connect(m_exaggSlider,    &QSlider::valueChanged, this, &SettingsPanel::onExaggerationChanged);
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
        m_renderer->setHeightScale(m_exaggSlider->value() / 100.0f);
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
    m_exaggSlider->setValue(100);  // 1.0× — transient, like the debug toggles
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

void SettingsPanel::onExaggerationChanged(int sliderValue)
{
    const double factor = sliderValue / 100.0;
    if (m_exaggValue)
        m_exaggValue->setText(QString::number(factor, 'f', 1) + QStringLiteral("×"));
    if (m_renderer)
        m_renderer->setHeightScale(static_cast<float>(factor));
}

void SettingsPanel::onAtlasSizeChanged(int)
{
    saveSettings();
}

void SettingsPanel::onResetCamera()
{
    if (m_renderer) m_renderer->resetView();
}

// ── Programmatic control (demo-capture mode) ────────────────────────────────────

void SettingsPanel::demoSetWireframe(bool on)    { m_wireframeCheck->setChecked(on); }
void SettingsPanel::demoSetNormals(bool on)      { m_normalsCheck->setChecked(on); }
void SettingsPanel::demoSetExaggeration(double factor)
{
    m_exaggSlider->setValue(static_cast<int>(factor * 100.0 + 0.5));
}

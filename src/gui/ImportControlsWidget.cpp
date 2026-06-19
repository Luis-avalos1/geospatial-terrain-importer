#include "ImportControlsWidget.hpp"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QGroupBox>

ImportControlsWidget::ImportControlsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);

    auto *grp = new QGroupBox("Import Options", this);
    auto *form = new QFormLayout(grp);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_resSpin = new QSpinBox(this);
    m_resSpin->setRange(0, 8192);
    m_resSpin->setSingleStep(64);
    m_resSpin->setValue(0);
    m_resSpin->setSpecialValueText("Native");
    m_resSpin->setToolTip("Target grid resolution (0 = use native resolution)");
    form->addRow("Resolution:", m_resSpin);

    m_heightSpin = new QDoubleSpinBox(this);
    m_heightSpin->setRange(0.001, 1000.0);
    m_heightSpin->setSingleStep(0.5);
    m_heightSpin->setValue(1.5);   // a touch of vertical exaggeration reads better by default
    m_heightSpin->setDecimals(2);
    m_heightSpin->setToolTip("Vertical exaggeration (1.0 = true scale)");
    form->addRow("Height scale:", m_heightSpin);

    m_lodSpin = new QSpinBox(this);
    m_lodSpin->setRange(1, 8);
    m_lodSpin->setValue(4);
    form->addRow("LOD levels:", m_lodSpin);

    m_simplifyCheck = new QCheckBox("Enable simplification", this);
    m_simplifyCheck->setChecked(false);
    form->addRow(m_simplifyCheck);

    m_simplifySpin = new QDoubleSpinBox(this);
    m_simplifySpin->setRange(0.05, 0.95);
    m_simplifySpin->setSingleStep(0.05);
    m_simplifySpin->setValue(0.5);
    m_simplifySpin->setDecimals(2);
    m_simplifySpin->setEnabled(false);
    form->addRow("Simplify ratio:", m_simplifySpin);

    m_atlasCheck = new QCheckBox("Build texture atlas", this);
    m_atlasCheck->setChecked(false);
    form->addRow(m_atlasCheck);

    connect(m_simplifyCheck, &QCheckBox::toggled,
            m_simplifySpin, &QDoubleSpinBox::setEnabled);

    outer->addWidget(grp);

    m_importBtn = new QPushButton("Import", this);
    m_importBtn->setObjectName("ImportButton");   // styled as the primary action
    m_importBtn->setDefault(true);
    outer->addWidget(m_importBtn);
    outer->addStretch();

    connect(m_importBtn, &QPushButton::clicked,
            this,        &ImportControlsWidget::importRequested);
}

ImportOptions ImportControlsWidget::options() const
{
    ImportOptions opts;
    opts.resolution    = m_resSpin->value();
    opts.heightScale   = static_cast<float>(m_heightSpin->value());
    opts.lodLevels     = m_lodSpin->value();
    opts.simplifyRatio = static_cast<float>(m_simplifySpin->value());
    opts.buildAtlas    = m_atlasCheck->isChecked();
    opts.runSimplifier = m_simplifyCheck->isChecked();
    return opts;
}

#pragma once

#include <QWidget>

class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QSlider;
class QPushButton;

struct ImportOptions {
    int   resolution    = 0;      // 0 = native
    float heightScale   = 1.0f;
    int   lodLevels     = 4;
    float simplifyRatio = 0.5f;
    bool  buildAtlas    = false;
    bool  runSimplifier = false;
};

class ImportControlsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImportControlsWidget(QWidget *parent = nullptr);

    ImportOptions options() const;

signals:
    void importRequested();

private:
    QSpinBox       *m_resSpin      = nullptr;
    QDoubleSpinBox *m_heightSpin   = nullptr;
    QSpinBox       *m_lodSpin      = nullptr;
    QDoubleSpinBox *m_simplifySpin = nullptr;
    QCheckBox      *m_atlasCheck   = nullptr;
    QCheckBox      *m_simplifyCheck = nullptr;
    QPushButton    *m_importBtn    = nullptr;
};

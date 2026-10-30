#pragma once

#include <QDialog>

#include "processing/morphology.h"

class QComboBox;
class QSpinBox;

namespace chameleon {

// Morphological analysis: dilation (cv::dilate) and erosion (cv::erode)
// with every function parameter settable (structuring element shape and
// size, anchor, iterations, border type and value).
class MorphologyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MorphologyDialog(QWidget *parent = nullptr);

    MorphologyParams morphologyParams() const;

signals:
    void applyRequested();

private:
    void updateMethodEnabled();

    QComboBox *m_operationCombo = nullptr;
    QComboBox *m_methodCombo = nullptr;
    QComboBox *m_shapeCombo = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QSpinBox *m_anchorXSpin = nullptr;
    QSpinBox *m_anchorYSpin = nullptr;
    QSpinBox *m_iterationsSpin = nullptr;
    QComboBox *m_borderCombo = nullptr;
    QSpinBox *m_borderValueSpin = nullptr;
};

} // namespace chameleon

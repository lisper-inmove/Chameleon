#pragma once

#include <QDialog>

#include "processing/binary.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace chameleon {

// Threshold segmentation dialog: thresh, maxval and the threshold type
// are settable. Applying is a one-shot, undoable operation on the current
// image (or selection).
class ThresholdDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ThresholdDialog(QWidget *parent = nullptr);

    ThresholdParams thresholdParams() const;
    // Shows the threshold the last apply computed (Otsu/Triangle)
    void setComputedThreshold(double value);

signals:
    void applyRequested();

private:
    void updateHint();
    void updateParameterVisibility();

    QComboBox *m_methodCombo = nullptr;
    QDoubleSpinBox *m_threshSpin = nullptr;
    QDoubleSpinBox *m_maxvalSpin = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QComboBox *m_adaptiveMethodCombo = nullptr;
    QSpinBox *m_blockSizeSpin = nullptr;
    QDoubleSpinBox *m_cSpin = nullptr;
    QLabel *m_computedLabel = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace chameleon

#pragma once

#include <QDialog>

#include "processing/filter_processor.h"

class QCheckBox;
class QLabel;
class QSpinBox;

namespace chameleon {

// Edge detection dialog (Canny): hysteresis thresholds, Sobel aperture
// and the L2-gradient switch are all settable. Applying is a one-shot,
// undoable operation on the current image (or selection).
class EdgeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EdgeDialog(QWidget *parent = nullptr);

    CannyParams cannyParams() const;

signals:
    void applyRequested();

private:
    QSpinBox *m_threshold1Spin = nullptr;
    QSpinBox *m_threshold2Spin = nullptr;
    QSpinBox *m_apertureSpin = nullptr;
    QCheckBox *m_l2GradientCheck = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace chameleon

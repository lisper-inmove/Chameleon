#pragma once

#include <QDialog>

#include "processing/filter_processor.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace chameleon {

// Gradient extraction dialog: Prewitt (filter2D kernels), Sobel and
// Scharr, with dx/dy/ksize/scale/delta exposed. Applying is a one-shot,
// undoable operation on the current image (or selection).
class GradientDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GradientDialog(QWidget *parent = nullptr);

    GradientParams gradientParams() const;
    QString currentTypeName() const;

signals:
    void applyRequested();

private:
    void updateParameterVisibility();

    QComboBox *m_typeCombo = nullptr;
    QSpinBox *m_dxSpin = nullptr;
    QSpinBox *m_dySpin = nullptr;
    QSpinBox *m_ksizeSpin = nullptr;
    QDoubleSpinBox *m_scaleSpin = nullptr;
    QDoubleSpinBox *m_deltaSpin = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace chameleon

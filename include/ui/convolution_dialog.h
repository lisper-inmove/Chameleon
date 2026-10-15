#pragma once

#include <array>

#include <QDialog>

#include "processing/filter_processor.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace chameleon {

// Convolution parameters dialog: box / Gaussian / median blur and a
// custom 3x3 filter2D kernel, with all settable parameters exposed.
// Applying is a one-shot, undoable operation on the current image
// (or selection); the dialog stays open for further tries.
class ConvolutionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConvolutionDialog(QWidget *parent = nullptr);

    BlurParams blurParams() const;
    QString currentTypeName() const;

signals:
    void applyRequested();

private:
    void updateParameterVisibility();

    QComboBox *m_typeCombo = nullptr;
    QSpinBox *m_kernelSpin = nullptr;
    QDoubleSpinBox *m_sigmaXSpin = nullptr;
    QDoubleSpinBox *m_sigmaYSpin = nullptr;
    QDoubleSpinBox *m_deltaSpin = nullptr;
    std::array<QDoubleSpinBox *, 9> m_kernelCells{};
    QDoubleSpinBox *m_sigmaColorSpin = nullptr;
    QDoubleSpinBox *m_sigmaSpaceSpin = nullptr;
    QComboBox *m_borderTypeCombo = nullptr;
    QWidget *m_kernelGrid = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace chameleon

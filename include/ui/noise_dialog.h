#pragma once

#include <QDialog>

#include "processing/filter_processor.h"
#include "processing/noise.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

namespace chameleon {

// Noise dialog with two groups: adding salt-pepper or Gaussian noise, and
// denoising with a mean or median filter (reusing the convolution
// implementations). Each action is a one-shot, undoable operation on the
// current image (or selection); the dialog stays open so noise can be
// added and then removed without reopening.
class NoiseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NoiseDialog(QWidget *parent = nullptr);

    bool saltPepperSelected() const;
    SaltPepperParams saltPepperParams() const;
    GaussianNoiseParams gaussianNoiseParams() const;
    BlurParams denoiseParams() const;

signals:
    void addNoiseRequested();
    void denoiseRequested();

private:
    void updateParameterVisibility();

    QComboBox *m_noiseTypeCombo = nullptr;
    QDoubleSpinBox *m_proportionSpin = nullptr;
    QDoubleSpinBox *m_meanSpin = nullptr;
    QDoubleSpinBox *m_sigmaSpin = nullptr;
    QComboBox *m_denoiseTypeCombo = nullptr;
    QSpinBox *m_denoiseKernelSpin = nullptr;
};

} // namespace chameleon

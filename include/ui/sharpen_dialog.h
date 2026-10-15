#pragma once

#include <QDialog>

#include "processing/filter_processor.h"

class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace chameleon {

// Laplacian sharpening dialog: kernel size and strength, plus a sharpness
// readout (variance of the Laplacian) of the current image or selection
// that refreshes on open, on demand and after every apply.
class SharpenDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SharpenDialog(QWidget *parent = nullptr);

    SharpenParams sharpenParams() const;
    void setSharpness(double variance);

signals:
    void applyRequested();
    void measureRequested();

private:
    QSpinBox *m_ksizeSpin = nullptr;
    QDoubleSpinBox *m_strengthSpin = nullptr;
    QLabel *m_sharpnessLabel = nullptr;
};

} // namespace chameleon

#pragma once

#include <QWidget>

class QSlider;

namespace chameleon {

// Filter parameter panel: brightness/contrast/saturation sliders with
// live preview, plus reset and apply buttons. The panel owns no state;
// it only forwards user input and mirrors resets coming from the viewer.
class FilterPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FilterPanel(QWidget *parent = nullptr);

public slots:
    // Zeroes the sliders without emitting (viewer-driven resets)
    void resetSliders();

signals:
    void filterParamsChanged(double brightness, double contrast, double saturation);
    void applyClicked();
    void resetClicked();

private:
    void emitParams();

    QSlider *m_brightness = nullptr;
    QSlider *m_contrast = nullptr;
    QSlider *m_saturation = nullptr;
};

} // namespace chameleon

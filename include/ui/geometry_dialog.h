#pragma once

#include <array>

#include <QDialog>
#include <QSize>

#include "processing/geometry.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace chameleon {

// Geometry dialog: cropping (to the current selection), rotation (any
// angle / 90-degree steps), flips, resampling on resize, four-point
// perspective correction and canvas resizing. Every apply is a
// base-changing, undoable operation on the current image.
class GeometryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit GeometryDialog(QWidget *parent = nullptr);

    GeometryParams geometryParams() const;
    // Seeds the size and quad fields from the current image/selection
    void setDefaults(const QSize &imageSize, const QRect &selection);

signals:
    void applyRequested();

private:
    void updateParameterVisibility();
    void updateHint();

    QComboBox *m_modeCombo = nullptr;
    QDoubleSpinBox *m_angleSpin = nullptr;
    QComboBox *m_flipCombo = nullptr;
    QSpinBox *m_widthSpin = nullptr;
    QSpinBox *m_heightSpin = nullptr;
    QComboBox *m_resampleCombo = nullptr;
    std::array<QDoubleSpinBox *, 4> m_quadX{};
    std::array<QDoubleSpinBox *, 4> m_quadY{};
    QLabel *m_hint = nullptr;
};

} // namespace chameleon

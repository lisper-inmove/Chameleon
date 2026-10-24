#pragma once

#include <QDialog>

#include "processing/hough.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QSpinBox;
class QTableWidget;

namespace chameleon {

// Hough transform dialog: line detection (cv::HoughLines) and circle
// detection (cv::HoughCircles). Every function parameter is exposed; the
// parameter group of the selected mode is enabled.
class HoughDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HoughDialog(QWidget *parent = nullptr);

    HoughMode mode() const;
    HoughLinesParams lineParams() const;
    HoughCirclesParams circleParams() const;
    void setHoughResult(const HoughResult &result, HoughMode mode);

signals:
    void applyRequested();

private:
    void updateModeVisibility();

    QComboBox *m_modeCombo = nullptr;
    // cv::HoughLines controls
    QGroupBox *m_lineGroup = nullptr;
    QDoubleSpinBox *m_rhoSpin = nullptr;
    QDoubleSpinBox *m_thetaSpin = nullptr;
    QSpinBox *m_thresholdSpin = nullptr;
    QDoubleSpinBox *m_srnSpin = nullptr;
    QDoubleSpinBox *m_stnSpin = nullptr;
    QDoubleSpinBox *m_minThetaSpin = nullptr;
    QDoubleSpinBox *m_maxThetaSpin = nullptr;
    QSpinBox *m_canny1Spin = nullptr;
    QSpinBox *m_canny2Spin = nullptr;
    QCheckBox *m_useEdgeValCheck = nullptr;
    QSpinBox *m_lineThicknessSpin = nullptr;
    // cv::HoughCircles controls
    QGroupBox *m_circleGroup = nullptr;
    QComboBox *m_methodCombo = nullptr;
    QDoubleSpinBox *m_dpSpin = nullptr;
    QDoubleSpinBox *m_minDistSpin = nullptr;
    QDoubleSpinBox *m_param1Spin = nullptr;
    QDoubleSpinBox *m_param2Spin = nullptr;
    QSpinBox *m_minRadiusSpin = nullptr;
    QSpinBox *m_maxRadiusSpin = nullptr;
    QSpinBox *m_circleThicknessSpin = nullptr;
    // Results
    QLabel *m_countLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
};

} // namespace chameleon

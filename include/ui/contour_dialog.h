#pragma once

#include <QDialog>

#include "processing/binary.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QTableWidget;

namespace chameleon {

// Contour detection dialog: every cv::findContours parameter (retrieval
// mode, approximation method, offset) plus the input binarization and the
// cv::drawContours options (thickness, line type, largest-only). Each fit
// function (cv::fitEllipse / fitLine / approxPolyDP) has its own button
// and its full parameter set is exposed.
class ContourDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ContourDialog(QWidget *parent = nullptr);

    ContourParams contourParams() const;
    // Fit parameters for the given method, read from the shared controls
    ContourFitParams contourFitParams(ContourFitType type) const;
    // Fills the results list: number, area, perimeter and bounding box
    void setContourInfos(const std::vector<ContourInfo> &infos);
    // Fills the fit results list for the given method
    void setContourFits(const std::vector<ContourFitInfo> &fits,
                        ContourFitType type);

signals:
    void applyRequested();
    void fitRequested(ContourFitType type);

private:
    void updateThresholdEnabled();

    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_methodCombo = nullptr;
    QSpinBox *m_offsetXSpin = nullptr;
    QSpinBox *m_offsetYSpin = nullptr;
    QSpinBox *m_thresholdSpin = nullptr;
    QCheckBox *m_otsuCheck = nullptr;
    QCheckBox *m_invertCheck = nullptr;
    QSpinBox *m_thicknessSpin = nullptr;
    QComboBox *m_lineTypeCombo = nullptr;
    QCheckBox *m_largestCheck = nullptr;
    QLabel *m_countLabel = nullptr;
    QTableWidget *m_statsTable = nullptr;
    // Fitting controls
    QComboBox *m_lineDistCombo = nullptr;
    QDoubleSpinBox *m_lineParamSpin = nullptr;
    QDoubleSpinBox *m_lineRepsSpin = nullptr;
    QDoubleSpinBox *m_lineAepsSpin = nullptr;
    QDoubleSpinBox *m_polyEpsilonSpin = nullptr;
    QCheckBox *m_polyClosedCheck = nullptr;
    QSpinBox *m_fitThicknessSpin = nullptr;
    QTableWidget *m_fitTable = nullptr;
};

} // namespace chameleon

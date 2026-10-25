#pragma once

#include <QDialog>

#include <opencv2/core.hpp>

#include "processing/contour_match.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QTableWidget;

namespace chameleon {

// Shape matching dialog: detects contours with cv::findContours, computes
// moments/Hu moments for each and scores them against the largest
// contour of a template image with cv::matchShapes. The template comes
// from the opened images or is browsed from disk.
class ContourMatchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ContourMatchDialog(const QStringList &templatePaths,
                                QWidget *parent = nullptr);

    // Adds a browsed file as template and selects it (no-op when already
    // present). Public so it is testable without the native file dialog.
    void addTargetPath(const QString &path);

    cv::Mat currentTemplateImage();
    QString currentTemplateName() const;
    ContourMatchParams contourMatchParams() const;
    void setResult(const ContourMatchResult &result);

signals:
    void applyRequested();

private:
    void updateTemplateInfo();

    cv::Mat m_loadedTemplate;
    QString m_loadedPath;
    QComboBox *m_targetCombo = nullptr;
    QComboBox *m_methodCombo = nullptr;
    QDoubleSpinBox *m_parameterSpin = nullptr;
    QLabel *m_infoLabel = nullptr;
    QLabel *m_bestLabel = nullptr;
    QTableWidget *m_table = nullptr;
};

} // namespace chameleon

#pragma once

#include <QDialog>

#include <opencv2/core.hpp>

class QComboBox;
class QLabel;
class QPushButton;

namespace chameleon {

// Compares the per-channel histograms of a reference image (the current
// image or selection) against a second input chosen from the opened
// images or browsed from disk, with cv::compareHist. The method is
// user-selectable and the result is shown as a normalized similarity in
// [0, 1] (1.0 = identical). Non-modal; closes and deletes itself.
class HistogramCompareDialog : public QDialog
{
    Q_OBJECT
public:
    HistogramCompareDialog(const cv::Mat &reference, const QString &referenceLabel,
                           const QStringList &targetPaths, QWidget *parent = nullptr);

    // Adds a browsed file as target input; selects it (no-op when already
    // present). Public so it is testable without the native file dialog.
    void addTargetPath(const QString &path);

private:
    void updateComparison();

    cv::Mat m_reference;
    cv::Mat m_loadedTarget;
    QString m_loadedPath;
    QComboBox *m_methodCombo = nullptr;
    QComboBox *m_targetCombo = nullptr;
    QLabel *m_resultsLabel = nullptr;
    QPushButton *m_browseButton = nullptr;
};

} // namespace chameleon

#pragma once

#include <QDialog>

#include <opencv2/core.hpp>

class QComboBox;
class QLabel;

namespace chameleon {

// Histogram matching: applies a transform to the current image (or
// selection) so its histogram matches a chosen template image. The
// template comes from the opened images or is browsed from disk.
// Non-modal; closes and deletes itself.
class HistogramMatchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HistogramMatchDialog(const QStringList &templatePaths,
                                  QWidget *parent = nullptr);

    // Adds a browsed file as template; selects it (no-op when already
    // present). Public so it is testable without the native file dialog.
    void addTargetPath(const QString &path);

    // Loaded on demand from the current selection
    cv::Mat currentTemplateImage();
    QString currentTemplateName() const;

signals:
    void applyRequested();

private:
    void updateTemplateInfo();

    cv::Mat m_loadedTemplate;
    QString m_loadedPath;
    QComboBox *m_targetCombo = nullptr;
    QLabel *m_infoLabel = nullptr;
};

} // namespace chameleon

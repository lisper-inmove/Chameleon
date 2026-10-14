#pragma once

#include <QDialog>

#include <opencv2/core.hpp>

class QComboBox;
class QLabel;

namespace chameleon {

// Histogram back projection: computes a match heat map of the current
// image (or selection) against a chosen template's histogram via
// cv::calcBackProject; the result opens in its own window. The template
// comes from the opened images or is browsed from disk. Non-modal;
// closes and deletes itself.
class BackProjectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BackProjectDialog(const QStringList &templatePaths,
                               QWidget *parent = nullptr);

    // Adds a browsed file as template; selects it (no-op when already
    // present). Public so it is testable without the native file dialog.
    void addTargetPath(const QString &path);

    cv::Mat currentTemplateImage();
    QString currentTemplateName() const;

signals:
    void computeRequested();

private:
    void updateTemplateInfo();

    cv::Mat m_loadedTemplate;
    QString m_loadedPath;
    QComboBox *m_targetCombo = nullptr;
    QLabel *m_infoLabel = nullptr;
};

} // namespace chameleon

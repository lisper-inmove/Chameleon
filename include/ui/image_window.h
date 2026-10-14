#pragma once

#include <QSize>
#include <QWidget>

#include <opencv2/core.hpp>

namespace chameleon {

// Simple top-level window showing an image scaled to fit, for computed
// results such as back projections. Closes and deletes itself.
class ImageWindow : public QWidget
{
    Q_OBJECT
public:
    ImageWindow(const QString &title, const cv::Mat &image, QWidget *parent = nullptr);

    QSize imageSize() const { return m_imageSize; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
    QSize m_imageSize;
};

} // namespace chameleon

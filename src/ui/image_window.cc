#include "ui/image_window.h"

#include <QPainter>

#include "ui/image_conversion.h"

namespace chameleon {

ImageWindow::ImageWindow(const QString &title, const cv::Mat &image, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_pixmap(pixmapFromMat(image))
    , m_imageSize(image.empty() ? QSize() : QSize(image.cols, image.rows))
{
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose);
    const QSize initial = m_imageSize.scaled(800, 600, Qt::KeepAspectRatio);
    resize(initial.expandedTo(QSize(240, 180)));
}

void ImageWindow::setImage(const QString &title, const cv::Mat &image)
{
    m_pixmap = pixmapFromMat(image);
    m_imageSize = image.empty() ? QSize() : QSize(image.cols, image.rows);
    setWindowTitle(title);
    update();
}

void ImageWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    if (m_pixmap.isNull())
        return;
    const QSize scaled = m_pixmap.size().scaled(size(), Qt::KeepAspectRatio);
    const QPixmap shown =
        m_pixmap.scaled(scaled, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(QPoint((width() - shown.width()) / 2, (height() - shown.height()) / 2),
                 shown);
}

} // namespace chameleon

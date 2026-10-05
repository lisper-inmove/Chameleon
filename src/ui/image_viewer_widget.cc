#include "ui/image_viewer_widget.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

#include "app/app_state.h"

namespace chameleon {

namespace {
constexpr double kMinScale = 0.1;
constexpr double kMaxScale = 8.0;
} // namespace

ImageViewerWidget::ImageViewerWidget(QWidget *parent) : QWidget(parent)
{
    connect(AppState::instance(), &AppState::zoomChanged, this, [this] {
        // Returning to Fit resets panning
        if (AppState::instance()->zoomMode() == AppState::Fit)
            m_pan = QPointF();
        update();
    });
}

void ImageViewerWidget::setImage(const QString &path)
{
    m_path = path;
    m_pan = QPointF();
    m_dragging = false;
    if (path.isEmpty()) {
        m_image = QImage();
        AppState::instance()->setImageInfo(QSize());
        m_status = NoImage;
        update();
        return;
    }
    m_status = Loading;
    update();

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        m_image = QImage();
        AppState::instance()->setImageInfo(QSize());
        m_status = Error;
    } else {
        m_image = image;
        AppState::instance()->setImageInfo(image.size());
        m_status = Ready;
    }
    update();
}

bool ImageViewerWidget::saveAs(const QString &path) const
{
    return m_status == Ready && m_image.save(path, "PNG");
}

double ImageViewerWidget::effectiveScale() const
{
    if (m_image.isNull())
        return 1.0;
    const auto *state = AppState::instance();
    switch (state->zoomMode()) {
    case AppState::Actual:
        return 1.0;
    case AppState::Custom:
        return state->zoomFactorForCurrent();
    case AppState::Fit:
    default:
        return std::min(double(width()) / m_image.width(),
                        double(height()) / m_image.height());
    }
}

QPointF ImageViewerWidget::imageTopLeft() const
{
    const double s = effectiveScale();
    return QPointF((width() - m_image.width() * s) / 2.0 + m_pan.x(),
                   (height() - m_image.height() * s) / 2.0 + m_pan.y());
}

void ImageViewerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());

    switch (m_status) {
    case NoImage:
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("按 Ctrl+O 打开图片"));
        return;
    case Error:
        p.setPen(Qt::red);
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("无法加载图片:%1").arg(QFileInfo(m_path).fileName()));
        return;
    case Loading:
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("加载中…"));
        return;
    case Ready:
        break;
    }

    const double s = effectiveScale();
    const QRectF target(imageTopLeft(), QSizeF(m_image.width() * s, m_image.height() * s));
    p.setRenderHint(QPainter::SmoothPixmapTransform, s < 2.0);
    p.drawImage(target, m_image);
}

void ImageViewerWidget::wheelEvent(QWheelEvent *event)
{
    if (m_status != Ready)
        return;
    auto *state = AppState::instance();
    const double oldScale = effectiveScale();
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
    const double newScale = std::clamp(oldScale * factor, kMinScale, kMaxScale);
    state->setZoomFactor(newScale);

    // Keep the image point under the cursor stationary
    const QPointF mouse = event->position();
    const QPointF tl = imageTopLeft();
    const QPointF imagePos((mouse.x() - tl.x()) / oldScale,
                           (mouse.y() - tl.y()) / oldScale);
    const QPointF center((width() - m_image.width() * newScale) / 2.0,
                         (height() - m_image.height() * newScale) / 2.0);
    m_pan = mouse - center - imagePos * newScale;
    update();
    event->accept();
}

void ImageViewerWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_status == Ready && event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastDragPos = event->position();
        event->accept();
    }
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_pan += event->position() - m_lastDragPos;
        m_lastDragPos = event->position();
        update();
        event->accept();
    }
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}

void ImageViewerWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_status != Ready)
        return;
    auto *state = AppState::instance();
    state->setZoomMode(state->zoomMode() == AppState::Fit
                           ? AppState::Actual : AppState::Fit);
    event->accept();
}

void ImageViewerWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

} // namespace chameleon

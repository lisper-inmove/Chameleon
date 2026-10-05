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
// Ruler strips outside the image canvas: top band (x ticks) and left
// band (y ticks), Photoshop-style
constexpr int kLeftRulerWidth = 44;
constexpr int kTopRulerHeight = 20;
} // namespace

ImageViewerWidget::ImageViewerWidget(QWidget *parent) : QWidget(parent)
{
    // Hover coordinate readout needs move events without a pressed button
    setMouseTracking(true);

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
    m_mouseInsideImage = false;
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
        // Initial state: image (0,0) coincides with the ruler origin
        alignToOrigin();
    }
    update();
}

bool ImageViewerWidget::saveAs(const QString &path) const
{
    return m_status == Ready && m_image.save(path, "PNG");
}

QRectF ImageViewerWidget::canvasRect() const
{
    return QRectF(kLeftRulerWidth, kTopRulerHeight,
                  width() - kLeftRulerWidth, height() - kTopRulerHeight);
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
        const QRectF canvas = canvasRect();
        return std::min(canvas.width() / m_image.width(),
                        canvas.height() / m_image.height());
    }
}

QPointF ImageViewerWidget::imageTopLeft() const
{
    const double s = effectiveScale();
    const QRectF canvas = canvasRect();
    return QPointF(canvas.left() + (canvas.width() - m_image.width() * s) / 2.0 + m_pan.x(),
                   canvas.top() + (canvas.height() - m_image.height() * s) / 2.0 + m_pan.y());
}

QPointF ImageViewerWidget::mapImageToView(const QPointF &imagePos) const
{
    const double s = effectiveScale();
    const QPointF tl = imageTopLeft();
    return QPointF(tl.x() + imagePos.x() * s, tl.y() + imagePos.y() * s);
}

void ImageViewerWidget::clampPan()
{
    if (m_image.isNull())
        return;
    const double s = effectiveScale();
    const QRectF canvas = canvasRect();
    const double rangeX = std::abs(m_image.width() * s - canvas.width()) / 2.0;
    const double rangeY = std::abs(m_image.height() * s - canvas.height()) / 2.0;
    m_pan.setX(std::clamp(m_pan.x(), -rangeX, rangeX));
    m_pan.setY(std::clamp(m_pan.y(), -rangeY, rangeY));
}

void ImageViewerWidget::alignToOrigin()
{
    if (m_image.isNull())
        return;
    const double s = effectiveScale();
    const QRectF canvas = canvasRect();
    m_pan = QPointF(-(canvas.width() - m_image.width() * s) / 2.0,
                    -(canvas.height() - m_image.height() * s) / 2.0);
}

void ImageViewerWidget::updateMousePosition(const QPointF &viewPos)
{
    m_lastMousePos = viewPos;
    if (m_status != Ready) {
        m_mouseInsideImage = false;
        return;
    }
    const double s = effectiveScale();
    const QPointF tl = imageTopLeft();
    const QPointF img((viewPos.x() - tl.x()) / s, (viewPos.y() - tl.y()) / s);
    m_mouseInsideImage = canvasRect().contains(viewPos)
            && img.x() >= 0 && img.y() >= 0
            && img.x() < m_image.width() && img.y() < m_image.height();
    if (m_mouseInsideImage)
        m_mouseImagePos = img;
    update();
}

void ImageViewerWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    const QRectF canvas = canvasRect();

    switch (m_status) {
    case NoImage:
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        p.drawText(canvas, Qt::AlignCenter, QStringLiteral("按 Ctrl+O 打开图片"));
        return;
    case Error:
        p.setPen(Qt::red);
        p.drawText(canvas, Qt::AlignCenter,
                   QStringLiteral("无法加载图片:%1").arg(QFileInfo(m_path).fileName()));
        return;
    case Loading:
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        p.drawText(canvas, Qt::AlignCenter, QStringLiteral("加载中…"));
        return;
    case Ready:
        break;
    }

    const double s = effectiveScale();
    const QPointF tl = imageTopLeft();
    const QRectF target(tl, QSizeF(m_image.width() * s, m_image.height() * s));

    // The image lives strictly inside the canvas; the ruler strips
    // outside it are never covered
    p.save();
    p.setClipRect(canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform, s < 2.0);
    p.drawImage(target, m_image);
    p.restore();

    // Ruler strips: separator lines along the canvas border
    p.setPen(QColor(100, 100, 100));
    p.drawLine(QPointF(canvas.left() - 1, 0), QPointF(canvas.left() - 1, height()));
    p.drawLine(QPointF(0, canvas.top() - 1), QPointF(width(), canvas.top() - 1));

    // Ticks in the strips: origin at the image top-left, x positive
    // right, y positive down, values in image pixels
    p.setPen(QColor(150, 150, 150));
    QFont tickFont = font();
    tickFont.setPointSizeF(std::max(7.0, tickFont.pointSizeF() * 0.8));
    p.setFont(tickFont);

    const double interval = niceTickInterval();
    for (double ix = 0; ix <= m_image.width(); ix += interval) {
        const double vx = tl.x() + ix * s;
        if (vx < -1 || vx > width() + 1)
            continue;
        p.drawLine(QPointF(vx, 0), QPointF(vx, kTopRulerHeight - 4));
        p.drawText(QRectF(vx + 3, 0, 40, kTopRulerHeight - 4),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(int(ix)));
    }
    for (double iy = 0; iy <= m_image.height(); iy += interval) {
        const double vy = tl.y() + iy * s;
        if (vy < -1 || vy > height() + 1)
            continue;
        p.drawLine(QPointF(0, vy), QPointF(kLeftRulerWidth - 4, vy));
        // Label sits below the tick line so they don't overlap
        p.drawText(QRectF(2, vy + 1, kLeftRulerWidth - 8, 16),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(int(iy)));
    }

    // Mouse position readout: image coordinates near the cursor
    if (m_mouseInsideImage) {
        const QString text = QStringLiteral("x=%1, y=%2")
                .arg(qRound(m_mouseImagePos.x()))
                .arg(qRound(m_mouseImagePos.y()));
        const QFontMetricsF metrics(p.font());
        const QRectF box(m_lastMousePos.x() + 14, m_lastMousePos.y() + 14,
                         metrics.horizontalAdvance(text) + 8, metrics.height() + 6);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 40, 40, 180));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(QColor(220, 220, 220));
        p.drawText(box, Qt::AlignCenter, text);
    }
}

double ImageViewerWidget::niceTickInterval() const
{
    const double s = effectiveScale();
    const int steps[] = {10, 25, 50, 100, 250, 500, 1000, 2500};
    for (int step : steps) {
        if (step * s >= 40.0)
            return step;
    }
    return 2500;
}

void ImageViewerWidget::wheelEvent(QWheelEvent *event)
{
    if (m_status != Ready)
        return;
    auto *state = AppState::instance();
    const double oldScale = effectiveScale();
    // Capture the top-left BEFORE the zoom state changes; the anchor math
    // below needs the old layout
    const QPointF tl = imageTopLeft();
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
    const double newScale = std::clamp(oldScale * factor, kMinScale, kMaxScale);
    state->setZoomFactor(newScale);

    // Keep the image point under the cursor stationary
    const QPointF mouse = event->position();
    const QPointF imagePos((mouse.x() - tl.x()) / oldScale,
                           (mouse.y() - tl.y()) / oldScale);
    const QRectF canvas = canvasRect();
    const QPointF center(canvas.left() + (canvas.width() - m_image.width() * newScale) / 2.0,
                         canvas.top() + (canvas.height() - m_image.height() * newScale) / 2.0);
    m_pan = mouse - center - imagePos * newScale;
    clampPan();
    updateMousePosition(mouse);
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
        clampPan();
        update();
        event->accept();
    }
    updateMousePosition(event->position());
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

void ImageViewerWidget::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_mouseInsideImage = false;
    update();
}

} // namespace chameleon

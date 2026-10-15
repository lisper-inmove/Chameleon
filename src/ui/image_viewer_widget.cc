#include "ui/image_viewer_widget.h"

#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

#include "app/app_state.h"
#include "processing/filter_processor.h"
#include "processing/histogram.h"
#include "processing/image_io.h"
#include "ui/image_conversion.h"

namespace chameleon {

namespace {
constexpr double kMinScale = 0.1;
constexpr double kMaxScale = 8.0;
// Ruler strips outside the image canvas: top band (x ticks) and left
// band (y ticks), Photoshop-style
constexpr int kLeftRulerWidth = 44;
constexpr int kTopRulerHeight = 20;
// Quiet period before a coalesced preview computation starts
constexpr int kFilterDebounceMs = 50;

// Normalizes uncommon bit depths to 8-bit so the rest of the pipeline
// (filters, display) works uniformly; 8-bit passes through untouched
cv::Mat normalizeTo8u(const cv::Mat &mat)
{
    if (mat.depth() == CV_8U)
        return mat;
    cv::Mat out;
    if (mat.depth() == CV_16U)
        mat.convertTo(out, CV_8U, 1.0 / 257.0);
    else if (mat.depth() == CV_32F || mat.depth() == CV_64F)
        mat.convertTo(out, CV_8U, 255.0);
    else
        return mat;
    return out;
}
} // namespace

ImageViewerWidget::ImageViewerWidget(QWidget *parent) : QWidget(parent)
{
    // Hover coordinate readout needs move events without a pressed button
    setMouseTracking(true);
    // Right-click handling is fully manual (drag distinguishes ROI
    // selection from the context menu), so no default context menu events
    setContextMenuPolicy(Qt::NoContextMenu);
    // Keyboard shortcuts (Esc clears the selection) arrive after a click
    setFocusPolicy(Qt::ClickFocus);

    connect(AppState::instance(), &AppState::zoomChanged, this, [this] {
        // Returning to Fit resets panning
        if (AppState::instance()->zoomMode() == AppState::Fit)
            m_pan = QPointF();
        update();
    });

    connect(&m_loadWatcher, &QFutureWatcher<cv::Mat>::finished, this, [this] {
        const QString requested = m_pendingPath;
        m_pendingPath.clear();
        // Ignore results superseded by a newer setImage() call
        if (requested.isEmpty() || requested != m_path)
            return;
        const cv::Mat mat = m_loadWatcher.result();
        if (mat.empty()) {
            m_image = cv::Mat();
            m_original = cv::Mat();
            updateDisplayPixmap();
            updateImageInfo();
            m_status = Error;
        } else {
            m_image = mat;
            m_original = mat;
            updateDisplayPixmap();
            updateImageInfo();
            m_status = Ready;
            // Initial state: image (0,0) coincides with the ruler origin
            alignToOrigin();
        }
        resetFilterState();
        update();
    });

    // Dedicated single-worker pool: a burst of parameter changes can never
    // occupy more than one core (the old implementation spawned one
    // global-pool task per change and pegged every core)
    m_filterPool = new QThreadPool(this);
    m_filterPool->setMaxThreadCount(1);

    // Coalesce slider-drag bursts: only compute after a short quiet period
    m_filterDebounce.setSingleShot(true);
    m_filterDebounce.setInterval(kFilterDebounceMs);
    connect(&m_filterDebounce, &QTimer::timeout,
            this, &ImageViewerWidget::startFilterPreview);

    connect(&m_filterWatcher, &QFutureWatcher<cv::Mat>::finished, this, [this] {
        m_filterBusy = false;
        const FilterParams done = m_runningParams;
        if (m_status != Ready)
            return;  // the image changed while computing; discard the result
        if (m_params.isNeutral()) {
            // Filters were reset while computing: show the original image
            m_image = m_original;
            updateDisplayPixmap();
            update();
            emit filterPreviewChanged();
            return;
        }
        if (!(m_pendingParams == done)) {
            // Newer parameters arrived while computing; skip the stale
            // result and compute the latest one instead
            if (!m_filterDebounce.isActive())
                startFilterPreview();
            return;
        }
        m_image = m_filterWatcher.result();
        updateDisplayPixmap();
        update();
        emit filterPreviewChanged();
    });
}

void ImageViewerWidget::startFilterPreview()
{
    if (m_status != Ready || m_pendingParams.isNeutral() || m_filterBusy)
        return;  // the finished handler picks up pending params when busy
    m_filterBusy = true;
    m_runningParams = m_pendingParams;
    // cv::Mat is refcounted; FilterProcessor never mutates its input, so
    // reading m_original on the worker thread is safe
    const cv::Mat source = m_original;
    const FilterParams params = m_runningParams;
    m_filterWatcher.setFuture(QtConcurrent::run(m_filterPool, [source, params] {
        return FilterProcessor::apply(source, params.brightness,
                                      params.contrast, params.saturation);
    }));
}

void ImageViewerWidget::setImage(const QString &path)
{
    m_path = path;
    m_pan = QPointF();
    m_dragging = false;
    m_mouseInsideImage = false;
    m_roiCandidate = false;
    m_roiDragging = false;
    if (m_selection.isEmpty() == false) {
        m_selection = QRect();
        emit selectionChanged();
    }
    if (path.isEmpty()) {
        m_pendingPath.clear();
        m_loadWatcher.setFuture(QFuture<cv::Mat>());  // cancel pending decode
        m_image = cv::Mat();
        m_original = cv::Mat();
        updateDisplayPixmap();
        updateImageInfo();
        m_status = NoImage;
        resetFilterState();
        update();
        return;
    }
    m_pendingPath = path;
    m_status = Loading;
    update();

    // Decode off the UI thread so the Loading state is real and large
    // images don't freeze the window; OpenCV handles the decode
    m_loadWatcher.setFuture(QtConcurrent::run([path] {
        return normalizeTo8u(ImageIO::imreadUtf8(path.toStdString()));
    }));
}

bool ImageViewerWidget::saveAs(const QString &path) const
{
    if (m_status != Ready || m_image.empty())
        return false;
    return ImageIO::imwriteUtf8(path.toStdString(), m_image);
}

void ImageViewerWidget::selectAll()
{
    if (m_image.empty())
        return;
    m_selection = QRect(0, 0, m_image.cols, m_image.rows);
    emit selectionChanged();
    update();
}

void ImageViewerWidget::clearSelection()
{
    if (m_selection.isEmpty())
        return;
    m_selection = QRect();
    emit selectionChanged();
    update();
}

cv::Rect ImageViewerWidget::selectionRect() const
{
    if (m_image.empty() || m_selection.isEmpty())
        return {};
    const QRect clamped = m_selection & QRect(0, 0, m_image.cols, m_image.rows);
    return cv::Rect(clamped.x(), clamped.y(), clamped.width(), clamped.height());
}

cv::Mat ImageViewerWidget::selectionImage() const
{
    const cv::Rect rect = selectionRect();
    if (rect.width <= 0 || rect.height <= 0)
        return {};
    return m_image(rect).clone();
}

QPointF ImageViewerWidget::viewToImage(const QPointF &viewPos) const
{
    const double s = effectiveScale();
    const QPointF tl = imageTopLeft();
    return QPointF((viewPos.x() - tl.x()) / s, (viewPos.y() - tl.y()) / s);
}

void ImageViewerWidget::updateDisplayPixmap()
{
    m_displayPixmap = pixmapFromMat(m_image);
}

void ImageViewerWidget::updateImageInfo()
{
    AppState::instance()->setImageInfo(
        m_image.empty() ? QSize() : QSize(m_image.cols, m_image.rows));
}

void ImageViewerWidget::resetFilterState()
{
    m_params = {};
    m_pendingParams = {};
    m_filterDebounce.stop();
    m_undoStack.clear();
    m_redoStack.clear();
    emit filterParamsReset();
    emit canUndoChanged(false);
    emit canRedoChanged(false);
}

bool ImageViewerWidget::hasActiveFilters() const
{
    return !m_params.isNeutral();
}

void ImageViewerWidget::setFilterParams(double brightness, double contrast,
                                        double saturation)
{
    if (m_status != Ready)
        return;
    m_params = {brightness, contrast, saturation};
    m_pendingParams = m_params;
    if (m_params.isNeutral()) {
        m_filterDebounce.stop();
        m_pendingParams = {};
        if (!m_filterBusy) {
            m_image = m_original;  // cv::Mat is refcounted, no pixel copy
            updateDisplayPixmap();
            update();
            emit filterPreviewChanged();
        }
        return;
    }
    m_filterDebounce.start();  // restarted on every change; coalesces bursts
}

void ImageViewerWidget::applyFilter()
{
    if (m_status != Ready || !hasActiveFilters())
        return;
    m_undoStack.push(m_original.clone());
    while (m_undoStack.size() > 10)
        m_undoStack.removeFirst();
    m_redoStack.clear();
    m_original = m_image;
    m_params = {};
    m_pendingParams = {};
    m_filterDebounce.stop();
    m_image = m_original;
    updateDisplayPixmap();
    update();
    emit filterPreviewChanged();
    emit filterParamsReset();
    emit canUndoChanged(true);
    emit canRedoChanged(false);
}

void ImageViewerWidget::equalizeHistogram()
{
    applyOperation([](const cv::Mat &image) {
        return chameleon::equalizeHistogram(image);
    });
}

void ImageViewerWidget::equalizeClahe()
{
    applyOperation([](const cv::Mat &image) {
        return chameleon::equalizeClahe(image);
    });
}

void ImageViewerWidget::matchHistogram(const cv::Mat &templateImage)
{
    // cv::Mat is refcounted; the copy is cheap and keeps the template
    // alive for the (synchronous) operation
    applyOperation([templateImage](const cv::Mat &image) {
        return chameleon::matchHistogram(image, templateImage);
    });
}

void ImageViewerWidget::applyConvolution(const BlurParams &params)
{
    applyOperation([params](const cv::Mat &image) {
        return chameleon::applyBlur(image, params);
    });
}

void ImageViewerWidget::applyOperation(
    const std::function<cv::Mat(const cv::Mat &)> &transform)
{
    if (m_status != Ready || m_image.empty())
        return;

    cv::Mat result;
    try {
        const cv::Rect rect = selectionRect();
        if (rect.width > 0 && rect.height > 0) {
            // Replace only the selected region
            result = m_image.clone();
            transform(m_image(rect)).copyTo(result(rect));
        } else {
            result = transform(m_image);
        }
    } catch (const cv::Exception &) {
        return;  // unsupported input; leave the image untouched
    }
    if (result.empty() || result.size() != m_image.size()
            || result.type() != m_image.type())
        return;

    m_undoStack.push(m_original.clone());
    while (m_undoStack.size() > 10)
        m_undoStack.removeFirst();
    m_redoStack.clear();
    m_original = std::move(result);
    m_params = {};
    m_pendingParams = {};
    m_filterDebounce.stop();
    m_image = m_original;
    updateDisplayPixmap();
    update();
    emit filterPreviewChanged();
    emit filterParamsReset();
    emit canUndoChanged(true);
    emit canRedoChanged(false);
}

void ImageViewerWidget::undo()
{
    if (m_status != Ready || m_undoStack.isEmpty())
        return;
    m_redoStack.push(m_original.clone());
    m_original = m_undoStack.pop();
    m_params = {};
    m_pendingParams = {};
    m_filterDebounce.stop();
    m_image = m_original;
    updateDisplayPixmap();
    update();
    emit filterPreviewChanged();
    emit filterParamsReset();
    emit canUndoChanged(!m_undoStack.isEmpty());
    emit canRedoChanged(true);
}

void ImageViewerWidget::redo()
{
    if (m_status != Ready || m_redoStack.isEmpty())
        return;
    m_undoStack.push(m_original.clone());
    m_original = m_redoStack.pop();
    m_params = {};
    m_pendingParams = {};
    m_filterDebounce.stop();
    m_image = m_original;
    updateDisplayPixmap();
    update();
    emit filterPreviewChanged();
    emit filterParamsReset();
    emit canUndoChanged(true);
    emit canRedoChanged(!m_redoStack.isEmpty());
}

QRectF ImageViewerWidget::canvasRect() const
{
    return QRectF(kLeftRulerWidth, kTopRulerHeight,
                  width() - kLeftRulerWidth, height() - kTopRulerHeight);
}

double ImageViewerWidget::effectiveScale() const
{
    if (m_image.empty())
        return 1.0;
    const auto *state = AppState::instance();
    switch (state->zoomMode()) {
    case AppState::Actual:
        return 1.0;
    case AppState::Custom:
        return state->zoomFactorForCurrent();
    case AppState::Fit:
    default: {
        const QRectF canvas = canvasRect();
        // Fit never exceeds the custom zoom range (kMaxScale); otherwise
        // tiny images would fit at >800% and zooming in would shrink them
        return std::min({canvas.width() / m_image.cols,
                         canvas.height() / m_image.rows, kMaxScale});
    }
    }
}

QPointF ImageViewerWidget::imageTopLeft() const
{
    const double s = effectiveScale();
    const QRectF canvas = canvasRect();
    return QPointF(canvas.left() + (canvas.width() - m_image.cols * s) / 2.0 + m_pan.x(),
                   canvas.top() + (canvas.height() - m_image.rows * s) / 2.0 + m_pan.y());
}

QPointF ImageViewerWidget::mapImageToView(const QPointF &imagePos) const
{
    const double s = effectiveScale();
    const QPointF tl = imageTopLeft();
    return QPointF(tl.x() + imagePos.x() * s, tl.y() + imagePos.y() * s);
}

void ImageViewerWidget::clampPan()
{
    if (m_image.empty())
        return;
    const double s = effectiveScale();
    const QRectF canvas = canvasRect();
    const double rangeX = std::abs(m_image.cols * s - canvas.width()) / 2.0;
    const double rangeY = std::abs(m_image.rows * s - canvas.height()) / 2.0;
    m_pan.setX(std::clamp(m_pan.x(), -rangeX, rangeX));
    m_pan.setY(std::clamp(m_pan.y(), -rangeY, rangeY));
}

void ImageViewerWidget::alignToOrigin()
{
    if (m_image.empty())
        return;
    const double s = effectiveScale();
    const QRectF canvas = canvasRect();
    m_pan = QPointF(-(canvas.width() - m_image.cols * s) / 2.0,
                    -(canvas.height() - m_image.rows * s) / 2.0);
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
            && img.x() < m_image.cols && img.y() < m_image.rows;
    if (m_mouseInsideImage)
        m_mouseImagePos = img;
    update();
}

void ImageViewerWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && hasSelection()) {
        clearSelection();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
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
    const QRectF target(tl, QSizeF(m_image.cols * s, m_image.rows * s));

    // The image lives strictly inside the canvas; the ruler strips
    // outside it are never covered
    p.save();
    p.setClipRect(canvas);
    p.setRenderHint(QPainter::SmoothPixmapTransform, s < 2.0);
    p.drawPixmap(target, m_displayPixmap, QRectF(m_displayPixmap.rect()));

    // ROI overlay: translucent fill + amber outline (image coordinates
    // scale with zoom/pan automatically)
    if (!m_selection.isEmpty()) {
        const QRectF selView(tl.x() + m_selection.x() * s, tl.y() + m_selection.y() * s,
                             m_selection.width() * s, m_selection.height() * s);
        p.setPen(QPen(QColor(255, 200, 0), 2));
        p.setBrush(QColor(255, 200, 0, 40));
        p.drawRect(selView);
    }
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
    for (int t = 0; t * interval <= m_image.cols; ++t) {
        const double ix = t * interval;
        const double vx = tl.x() + ix * s;
        if (vx < -1 || vx > width() + 1)
            continue;
        p.drawLine(QPointF(vx, 0), QPointF(vx, kTopRulerHeight - 4));
        p.drawText(QRectF(vx + 3, 0, 40, kTopRulerHeight - 4),
                   Qt::AlignLeft | Qt::AlignVCenter, QString::number(int(ix)));
    }
    for (int t = 0; t * interval <= m_image.rows; ++t) {
        const double iy = t * interval;
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
    // Ignore purely horizontal wheel/trackpad swipes
    if (event->angleDelta().y() == 0)
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
    const QPointF center(canvas.left() + (canvas.width() - m_image.cols * newScale) / 2.0,
                         canvas.top() + (canvas.height() - m_image.rows * newScale) / 2.0);
    m_pan = mouse - center - imagePos * newScale;
    clampPan();
    updateMousePosition(mouse);
    update();
    event->accept();
}

void ImageViewerWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_status != Ready)
        return;
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastDragPos = event->position();
        event->accept();
    } else if (event->button() == Qt::RightButton) {
        // Candidate: becomes a ROI drag on movement, a context menu on
        // release without movement
        m_roiCandidate = true;
        m_roiDragging = false;
        m_roiPressView = event->position();
        m_roiAnchorImage = viewToImage(event->position());
        event->accept();
    }
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_roiCandidate) {
        if (!m_roiDragging
                && (event->position() - m_roiPressView).manhattanLength()
                       >= QApplication::startDragDistance()) {
            m_roiDragging = true;
        }
        if (m_roiDragging) {
            const QPointF current = viewToImage(event->position());
            const QRectF rect = QRectF(m_roiAnchorImage, current).normalized();
            const int x1 = std::clamp(int(std::lround(rect.left())), 0, m_image.cols);
            const int y1 = std::clamp(int(std::lround(rect.top())), 0, m_image.rows);
            const int x2 = std::clamp(int(std::lround(rect.right())), 0, m_image.cols);
            const int y2 = std::clamp(int(std::lround(rect.bottom())), 0, m_image.rows);
            // Top-left + size construction: edges x2/y2 are exclusive
            // (max valid pixel is cols-1/rows-1)
            m_selection = QRect(x1, y1, x2 - x1, y2 - y1);
            emit selectionChanged();
            update();
            event->accept();
            return;
        }
    }
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
    } else if (m_roiCandidate && event->button() == Qt::RightButton) {
        m_roiCandidate = false;
        if (m_roiDragging) {
            m_roiDragging = false;
            // A degenerate rubber band (a few pixels) is not a useful
            // selection; treat it as an accidental move
            if (m_selection.width() < 3 || m_selection.height() < 3) {
                m_selection = QRect();
            }
            emit selectionChanged();
            update();
        } else {
            emit contextMenuRequested(event->globalPosition().toPoint());
        }
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

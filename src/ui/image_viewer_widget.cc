#include "ui/image_viewer_widget.h"

#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <utility>

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
        if (m_independentZoom)
            return;  // local zoom state; shared changes do not apply here
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
            m_sourceImage = cv::Mat();
            m_image = cv::Mat();
            m_original = cv::Mat();
            updateDisplayPixmap();
            updateImageInfo();
            m_status = Error;
        } else {
            m_sourceImage = mat;
            m_image = mat;
            m_original = mat;
            updateDisplayPixmap();
            updateImageInfo();
            m_status = Ready;
            // Initial state: image (0,0) coincides with the ruler origin
            alignToOrigin();
        }
        resetFilterState();
        m_baseUndoStack.clear();
        m_baseRedoStack.clear();
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

void ImageViewerWidget::setIndependentZoom(bool enabled)
{
    m_independentZoom = enabled;
    if (!enabled)
        m_localZoomByPath.clear();
    update();
}

AppState::ZoomMode ImageViewerWidget::zoomMode() const
{
    if (m_independentZoom)
        return m_localZoomMode;
    return AppState::instance()->zoomMode();
}

double ImageViewerWidget::zoomFactor() const
{
    if (m_independentZoom)
        return m_localZoomMode == AppState::Custom ? m_localZoomFactor : 1.0;
    return AppState::instance()->zoomFactorForCurrent();
}

void ImageViewerWidget::setZoomMode(AppState::ZoomMode mode)
{
    if (!m_independentZoom) {
        AppState::instance()->setZoomMode(mode);
        return;
    }
    m_localZoomMode = mode;
    if (mode == AppState::Fit)
        m_pan = QPointF();  // Fit is anchored top-left; drop the old pan
    rememberLocalZoom();
    update();
}

void ImageViewerWidget::setZoomFactor(double factor)
{
    const double clamped = std::clamp(factor, kMinScale, kMaxScale);
    if (!m_independentZoom) {
        AppState::instance()->setZoomFactor(clamped);
        return;
    }
    m_localZoomFactor = clamped;
    m_localZoomMode = AppState::Custom;
    rememberLocalZoom();
    update();
}

void ImageViewerWidget::rememberLocalZoom()
{
    if (!m_independentZoom || m_path.isEmpty())
        return;
    m_localZoomByPath.insert(m_path, LocalZoom{m_localZoomMode, m_localZoomFactor});
}

void ImageViewerWidget::setImage(const QString &path)
{
    // Independent zoom is remembered per image, like the processed pane's
    // AppState zoom: switching tabs must not lose the original pane's zoom
    if (m_independentZoom && path != m_path) {
        rememberLocalZoom();
        const auto it = m_localZoomByPath.constFind(path);
        if (it != m_localZoomByPath.constEnd()) {
            m_localZoomMode = it->mode;
            m_localZoomFactor = it->factor;
        } else {
            m_localZoomMode = AppState::Fit;
            m_localZoomFactor = 1.0;
        }
    }

    // Remember the outgoing image's processed state (only for images that
    // are still open; a closed image is forgotten)
    if (m_rememberState && m_status == Ready && !m_path.isEmpty()
            && m_path != path
            && AppState::instance()->images().contains(m_path)) {
        m_stateByPath.insert(m_path, RememberedState{
                                          m_sourceImage, m_original, m_image,
                                          m_undoStack, m_redoStack,
                                          m_baseUndoStack, m_baseRedoStack,
                                          m_selection});
    }

    // Restore a remembered state synchronously: no decode, edits intact
    if (m_rememberState && !path.isEmpty()) {
        const auto it = m_stateByPath.constFind(path);
        if (it != m_stateByPath.constEnd()) {
            m_path = path;
            m_pan = QPointF();
            m_dragging = false;
            m_roiCandidate = false;
            m_roiDragging = false;
            m_mouseInsideImage = false;
            m_pendingPath.clear();
            m_loadWatcher.setFuture(QFuture<cv::Mat>());  // cancel pending decode
            m_sourceImage = it->source;
            m_original = it->original;
            m_image = it->image;
            m_selection = it->selection;
            m_status = Ready;
            resetFilterState();  // live preview params reset (also clears stacks)
            m_undoStack = it->undo;
            m_redoStack = it->redo;
            m_baseUndoStack = it->baseUndo;
            m_baseRedoStack = it->baseRedo;
            emit canUndoChanged(!m_undoStack.isEmpty());
            emit canRedoChanged(!m_redoStack.isEmpty());
            if (!m_selection.isEmpty())
                emit selectionChanged();
            updateDisplayPixmap();
            updateImageInfo();
            alignToOrigin();
            update();
            return;
        }
    }

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
        m_sourceImage = cv::Mat();
        m_image = cv::Mat();
        m_original = cv::Mat();
        updateDisplayPixmap();
        updateImageInfo();
        m_status = NoImage;
        resetFilterState();
        m_baseUndoStack.clear();
        m_baseRedoStack.clear();
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

void ImageViewerWidget::setRememberStatePerPath(bool enabled)
{
    m_rememberState = enabled;
    if (!enabled)
        m_stateByPath.clear();
}

void ImageViewerWidget::retainRememberedStates(const QStringList &paths)
{
    for (auto it = m_stateByPath.begin(); it != m_stateByPath.end();) {
        if (paths.contains(it.key()))
            ++it;
        else
            it = m_stateByPath.erase(it);
    }
    for (auto it = m_localZoomByPath.begin(); it != m_localZoomByPath.end();) {
        if (paths.contains(it.key()))
            ++it;
        else
            it = m_localZoomByPath.erase(it);
    }
}

void ImageViewerWidget::reloadImage()
{
    if (m_path.isEmpty())
        return;
    // Drop the remembered edits (and the per-image zoom) and bypass the
    // restore branch so the file is decoded fresh
    m_stateByPath.remove(m_path);
    m_localZoomByPath.remove(m_path);
    m_localZoomMode = AppState::Fit;
    m_localZoomFactor = 1.0;
    const QString path = m_path;
    m_path.clear();
    m_status = NoImage;
    setImage(path);
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

void ImageViewerWidget::applyGradient(const GradientParams &params)
{
    applyOperation([params](const cv::Mat &image) {
        return chameleon::applyGradient(image, params);
    });
}

void ImageViewerWidget::applyEdgeDetection(const CannyParams &params)
{
    applyOperation([params](const cv::Mat &image) {
        return chameleon::applyCanny(image, params);
    });
}

void ImageViewerWidget::applySharpening(const SharpenParams &params)
{
    applyOperation([params](const cv::Mat &image) {
        return chameleon::applyLaplacianSharpen(image, params);
    });
}

double ImageViewerWidget::applyThreshold(const ThresholdParams &params)
{
    // applyOperation invokes the transform synchronously, so reading the
    // captured value afterwards is safe
    double computed = params.thresh;
    applyOperation([params, &computed](const cv::Mat &image) {
        return chameleon::applyThreshold(image, params, &computed);
    });
    return computed;
}

std::vector<ContourInfo> ImageViewerWidget::applyContours(
    const ContourParams &params)
{
    // applyOperation transforms synchronously, so the captured infos are
    // valid when it returns
    std::vector<ContourInfo> infos;
    applyOperation([params, &infos](const cv::Mat &image) {
        const ContourResult result =
            chameleon::findAndDrawContours(image, params);
        infos = result.infos;
        return result.image;
    });
    return infos;
}

ContourFitResult ImageViewerWidget::applyContourFit(
    const ContourParams &contourParams, const ContourFitParams &fitParams)
{
    // applyOperation transforms synchronously, so the captured result is
    // valid when it returns
    ContourFitResult result;
    applyOperation([&contourParams, &fitParams, &result](const cv::Mat &image) {
        result = chameleon::findAndFitContours(image, contourParams, fitParams);
        return result.image;
    });
    return result;
}

HoughResult ImageViewerWidget::applyHoughLines(
    const HoughLinesParams &params)
{
    HoughResult result;
    applyOperation([&params, &result](const cv::Mat &image) {
        result = chameleon::houghLines(image, params);
        return result.image;
    });
    return result;
}

HoughResult ImageViewerWidget::applyHoughCircles(
    const HoughCirclesParams &params)
{
    HoughResult result;
    applyOperation([&params, &result](const cv::Mat &image) {
        result = chameleon::houghCircles(image, params);
        return result.image;
    });
    return result;
}

ContourMatchResult ImageViewerWidget::applyContourMatch(
    const cv::Mat &templateImage, const ContourMatchParams &params)
{
    ContourMatchResult result;
    applyOperation([&templateImage, &params, &result](const cv::Mat &image) {
        result = chameleon::matchContours(image, templateImage, params);
        return result.image;
    });
    return result;
}

ConvexHullResult ImageViewerWidget::applyConvexHull(
    const ContourParams &contourParams, const ConvexHullParams &hullParams)
{
    ConvexHullResult result;
    applyOperation([&contourParams, &hullParams, &result](const cv::Mat &image) {
        result = chameleon::findConvexHulls(image, contourParams, hullParams);
        return result.image;
    });
    return result;
}

void ImageViewerWidget::addSaltPepperNoise(const SaltPepperParams &params)
{
    // A fresh random seed per invocation; repeated calls differ, undo
    // still restores exactly
    applyOperation([params](const cv::Mat &image) {
        return chameleon::addSaltPepperNoise(image, params);
    });
}

void ImageViewerWidget::addGaussianNoise(const GaussianNoiseParams &params)
{
    applyOperation([params](const cv::Mat &image) {
        return chameleon::addGaussianNoise(image, params);
    });
}

void ImageViewerWidget::applyGeometry(const GeometryParams &params)
{
    if (m_status != Ready || m_sourceImage.empty())
        return;
    switch (params.operation) {
    case GeometryOperation::Crop: {
        if (!hasSelection())
            return;
        const cv::Rect rect = selectionRect();
        if (rect.width <= 0 || rect.height <= 0)
            return;
        applyGeometryOperation([rect](const cv::Mat &source) {
            return source(rect).clone();
        });
        break;
    }
    case GeometryOperation::Rotate:
        // Rotation must not zoom the view: the rotated canvas usually has
        // another aspect ratio, and a Fit refit would visibly rescale it
        applyGeometryOperation(
            [angle = params.angle](const cv::Mat &source) {
                return chameleon::rotateImage(source, angle);
            },
            true);
        break;
    case GeometryOperation::Flip:
        applyGeometryOperation([direction = params.flip](const cv::Mat &source) {
            return chameleon::flipImage(source, direction);
        });
        break;
    case GeometryOperation::Resize:
        applyGeometryOperation([width = params.width, height = params.height,
                                method = params.resample](const cv::Mat &source) {
            return chameleon::resizeImage(source, width, height, method);
        });
        break;
    case GeometryOperation::Perspective:
        applyGeometryOperation([quad = params.quad](const cv::Mat &source) {
            return chameleon::perspectiveCorrect(source, quad);
        });
        break;
    case GeometryOperation::Canvas:
        applyGeometryOperation([width = params.width, height = params.height](
                                   const cv::Mat &source) {
            return chameleon::adjustCanvas(source, width, height);
        });
        break;
    }
}

void ImageViewerWidget::applyGeometryOperation(
    const std::function<cv::Mat(const cv::Mat &)> &transform, bool preserveViewScale)
{
    // Capture the displayed magnification before the image is replaced
    const bool wasFit = zoomMode() == AppState::Fit;
    const double scaleBefore = wasFit ? effectiveScale() : 1.0;
    const cv::Size sizeBefore = m_sourceImage.size();

    cv::Mat result;
    try {
        result = transform(m_sourceImage);
    } catch (const cv::Exception &) {
        return;
    }
    if (result.empty())
        return;

    m_baseUndoStack.push(GeometryCheckpoint{m_sourceImage, m_image, m_selection});
    while (m_baseUndoStack.size() > 10)
        m_baseUndoStack.removeFirst();
    m_baseRedoStack.clear();

    m_sourceImage = result;
    m_original = result;
    m_image = std::move(result);
    if (!m_selection.isEmpty()) {
        m_selection = QRect();
        emit selectionChanged();
    }
    if (preserveViewScale && wasFit && m_image.size() != sizeBefore) {
        // Leave Fit for a fixed factor at the previous scale; re-centre
        m_pan = QPointF();
        setZoomFactor(scaleBefore);
    }
    resetFilterState();  // filter params and image-level history belong to the old base
    updateDisplayPixmap();
    updateImageInfo();
    update();
    emit filterPreviewChanged();
    emit canUndoChanged(true);
    emit canRedoChanged(false);
}

bool ImageViewerWidget::canUndoAny() const
{
    return !m_undoStack.isEmpty() || !m_baseUndoStack.isEmpty();
}

bool ImageViewerWidget::canRedoAny() const
{
    return !m_redoStack.isEmpty() || !m_baseRedoStack.isEmpty();
}

void ImageViewerWidget::applyOperation(
    const std::function<cv::Mat(const cv::Mat &)> &transform)
{
    if (m_status != Ready || m_image.empty())
        return;

    // Toolbar operations always transform the pristine original, never a
    // previous result: applying another operation replaces the displayed
    // image instead of compounding
    if (m_sourceImage.empty())
        return;
    cv::Mat result;
    try {
        const cv::Rect rect = selectionRect();
        if (rect.width > 0 && rect.height > 0) {
            // Replace only the selected region of the original
            result = m_sourceImage.clone();
            transform(m_sourceImage(rect)).copyTo(result(rect));
        } else {
            result = transform(m_sourceImage);
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
    if (m_status != Ready)
        return;
    if (m_undoStack.isEmpty()) {
        if (m_baseUndoStack.isEmpty())
            return;
        m_baseRedoStack.push(GeometryCheckpoint{m_sourceImage, m_image, m_selection});
        const GeometryCheckpoint state = m_baseUndoStack.pop();
        m_sourceImage = state.source;
        m_original = state.image;
        m_image = state.image;
        m_selection = state.selection;
        resetFilterState();
        updateDisplayPixmap();
        updateImageInfo();
        emit selectionChanged();
        emit canUndoChanged(canUndoAny());
        emit canRedoChanged(true);
        update();
        return;
    }
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
    emit canUndoChanged(canUndoAny());
    emit canRedoChanged(true);
}

void ImageViewerWidget::redo()
{
    if (m_status != Ready)
        return;
    if (m_redoStack.isEmpty()) {
        if (m_baseRedoStack.isEmpty())
            return;
        m_baseUndoStack.push(GeometryCheckpoint{m_sourceImage, m_image, m_selection});
        const GeometryCheckpoint state = m_baseRedoStack.pop();
        m_sourceImage = state.source;
        m_original = state.image;
        m_image = state.image;
        m_selection = state.selection;
        resetFilterState();
        updateDisplayPixmap();
        updateImageInfo();
        emit selectionChanged();
        emit canUndoChanged(true);
        emit canRedoChanged(canRedoAny());
        update();
        return;
    }
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
    emit canRedoChanged(canRedoAny());
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
    switch (zoomMode()) {
    case AppState::Actual:
        return 1.0;
    case AppState::Custom:
        return zoomFactor();
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
    if (zoomMode() == AppState::Fit) {
        // Fit never centres: the image is anchored to the canvas top-left
        // (any pan nudges it within the leftover space)
        return QPointF(canvas.left() + m_pan.x(), canvas.top() + m_pan.y());
    }
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
    if (zoomMode() == AppState::Fit) {
        // Top-left anchored: pan moves within the leftover space only, so
        // the image never leaves the canvas
        const double slackX = std::max(0.0, canvas.width() - m_image.cols * s);
        const double slackY = std::max(0.0, canvas.height() - m_image.rows * s);
        m_pan.setX(std::clamp(m_pan.x(), 0.0, slackX));
        m_pan.setY(std::clamp(m_pan.y(), 0.0, slackY));
        return;
    }
    const double rangeX = std::abs(m_image.cols * s - canvas.width()) / 2.0;
    const double rangeY = std::abs(m_image.rows * s - canvas.height()) / 2.0;
    m_pan.setX(std::clamp(m_pan.x(), -rangeX, rangeX));
    m_pan.setY(std::clamp(m_pan.y(), -rangeY, rangeY));
}

void ImageViewerWidget::alignToOrigin()
{
    if (m_image.empty())
        return;
    if (zoomMode() == AppState::Fit) {
        m_pan = QPointF();  // the top-left anchor already coincides
        return;
    }
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
        m_mouseGray = -1;
        return;
    }
    const double s = effectiveScale();
    const QPointF tl = imageTopLeft();
    const QPointF img((viewPos.x() - tl.x()) / s, (viewPos.y() - tl.y()) / s);
    m_mouseInsideImage = canvasRect().contains(viewPos)
            && img.x() >= 0 && img.y() >= 0
            && img.x() < m_image.cols && img.y() < m_image.rows;
    if (m_mouseInsideImage) {
        m_mouseImagePos = img;
        const int x = std::clamp(int(img.x()), 0, m_image.cols - 1);
        const int y = std::clamp(int(img.y()), 0, m_image.rows - 1);
        switch (m_image.channels()) {
        case 1:
            m_mouseGray = m_image.at<uchar>(y, x);
            break;
        case 3: {
            const cv::Vec3b p = m_image.at<cv::Vec3b>(y, x);
            m_mouseGray = cvRound(0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0]);
            break;
        }
        case 4: {
            const cv::Vec4b p = m_image.at<cv::Vec4b>(y, x);
            m_mouseGray = cvRound(0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0]);
            break;
        }
        default:
            m_mouseGray = -1;
            break;
        }
    } else {
        m_mouseGray = -1;
    }
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
        const QString text = QStringLiteral("x=%1, y=%2, 灰度=%3")
                .arg(qRound(m_mouseImagePos.x()))
                .arg(qRound(m_mouseImagePos.y()))
                .arg(m_mouseGray);
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
    const double oldScale = effectiveScale();
    // Capture the top-left BEFORE the zoom state changes; the anchor math
    // below needs the old layout
    const QPointF tl = imageTopLeft();
    const double factor = event->angleDelta().y() > 0 ? 1.15 : 1 / 1.15;
    const double newScale = std::clamp(oldScale * factor, kMinScale, kMaxScale);
    setZoomFactor(newScale);  // local when independent, shared otherwise

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
    setZoomMode(zoomMode() == AppState::Fit ? AppState::Actual
                                            : AppState::Fit);
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
    m_mouseGray = -1;
    update();
}

} // namespace chameleon

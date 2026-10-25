#pragma once

#include <functional>

#include <QFutureWatcher>
#include <QHash>
#include <QPointF>
#include <QPixmap>
#include <QRect>
#include <QStack>
#include <QThreadPool>
#include <QTimer>
#include <QWidget>

#include <opencv2/core.hpp>

#include "app/app_state.h"
#include "processing/binary.h"
#include "processing/contour_match.h"
#include "processing/filter_processor.h"
#include "processing/geometry.h"
#include "processing/hough.h"
#include "processing/noise.h"

namespace chameleon {

// Central image display: zoom/pan via paintEvent, empty/loading/error states.
// Image data and all image operations are OpenCV-only (cv::Mat); Qt is used
// for presentation (widget, events, the converted display cache).
class ImageViewerWidget : public QWidget
{
    Q_OBJECT
public:
    enum Status { NoImage, Loading, Ready, Error };

    explicit ImageViewerWidget(QWidget *parent = nullptr);

    Status status() const { return m_status; }
    QString imagePath() const { return m_path; }
    QPointF panOffset() const { return m_pan; }
    QPointF mouseImagePos() const { return m_mouseImagePos; }
    bool mouseInsideImage() const { return m_mouseInsideImage; }
    // Grayscale value under the cursor; -1 when outside the image
    int mouseGrayValue() const { return m_mouseGray; }

    // Maps a point in image pixel coordinates to view coordinates
    QPointF mapImageToView(const QPointF &imagePos) const;

    // By default zoom follows AppState's shared state. With independent
    // zoom enabled this viewer keeps its own mode/factor (remembered per
    // image path), so the original and processed panes zoom separately.
    void setIndependentZoom(bool enabled);
    bool independentZoom() const { return m_independentZoom; }
    AppState::ZoomMode zoomMode() const;
    double zoomFactor() const;
    void setZoomMode(AppState::ZoomMode mode);
    void setZoomFactor(double factor);

    // Region of interest, in image pixel coordinates; empty when none
    QRect selection() const { return m_selection; }
    bool hasSelection() const { return !m_selection.isEmpty(); }
    void selectAll();
    void clearSelection();
    // Crop of the displayed image at the selection; empty when none
    cv::Mat selectionImage() const;
    // The displayed image (original + applied filters), as shown
    cv::Mat currentImage() const { return m_image; }

    // "" clears back to the empty state
    void setImage(const QString &path);
    bool saveAs(const QString &path) const;

    // When enabled, the processed state (current image, undo/redo stacks,
    // selection) is remembered per path, so switching tabs and back
    // restores the edits instead of re-reading the file. Images no longer
    // in AppState's list are forgotten (see retainRememberedStates()).
    void setRememberStatePerPath(bool enabled);
    // Drops remembered entries for paths outside the given list
    void retainRememberedStates(const QStringList &paths);
    // Discards the current image's remembered edits and re-reads the file
    void reloadImage();

    // Filter pipeline: params live-preview on a background thread;
    // applyFilter() bakes the preview into the image and pushes the
    // previous state onto the undo stack
    void setFilterParams(double brightness, double contrast, double saturation);
    void applyFilter();
    void undo();
    void redo();
    bool hasActiveFilters() const;

    // One-shot histogram equalization (global / CLAHE) of the displayed
    // image, or of the selection when one exists; undoable
    void equalizeHistogram();
    void equalizeClahe();
    // Histogram matching to the given template image; undoable
    void matchHistogram(const cv::Mat &templateImage);
    // One-shot convolution/blur; undoable
    void applyConvolution(const BlurParams &params);
    // One-shot gradient extraction (Prewitt / Sobel / Scharr); undoable
    void applyGradient(const GradientParams &params);
    // One-shot Canny edge detection; undoable
    void applyEdgeDetection(const CannyParams &params);
    // One-shot Laplacian sharpening; undoable
    void applySharpening(const SharpenParams &params);
    // One-shot threshold segmentation; undoable. Returns the threshold
    // cv::threshold computed for the automatic methods (params.thresh for
    // the fixed method).
    double applyThreshold(const ThresholdParams &params);
    // One-shot contour detection + drawing (cv::findContours /
    // cv::drawContours); undoable. Returns the per-contour measurements.
    std::vector<ContourInfo> applyContours(const ContourParams &params);
    // One-shot contour fitting (cv::fitEllipse / fitLine / approxPolyDP)
    // drawn over the contours; undoable. Returns the measurements and fits.
    ContourFitResult applyContourFit(const ContourParams &contourParams,
                                     const ContourFitParams &fitParams);
    // One-shot Hough transform detection; undoable
    HoughResult applyHoughLines(const HoughLinesParams &params);
    HoughResult applyHoughCircles(const HoughCirclesParams &params);
    // One-shot contour shape matching against a template image; undoable
    ContourMatchResult applyContourMatch(const cv::Mat &templateImage,
                                         const ContourMatchParams &params);
    // One-shot noise addition; undoable
    void addSaltPepperNoise(const SaltPepperParams &params);
    void addGaussianNoise(const GaussianNoiseParams &params);
    // Geometric transform (may change the size): the result becomes the
    // new original base; undoable through the base-change stack
    void applyGeometry(const GeometryParams &params);

signals:
    void selectionChanged();
    // Emitted on a right-click without drag; the receiver shows the menu
    void contextMenuRequested(const QPoint &globalPos);
    void filterPreviewChanged();
    void filterParamsReset();
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF canvasRect() const;
    double effectiveScale() const;
    QPointF imageTopLeft() const;
    double niceTickInterval() const;
    void clampPan();
    void alignToOrigin();
    QPointF viewToImage(const QPointF &viewPos) const;
    void updateMousePosition(const QPointF &viewPos);
    void updateDisplayPixmap();
    void updateImageInfo();
    void rememberLocalZoom();

    struct FilterParams {
        double brightness = 0.0;
        double contrast = 0.0;
        double saturation = 0.0;
        bool operator==(const FilterParams &other) const
        {
            return brightness == other.brightness
                    && contrast == other.contrast
                    && saturation == other.saturation;
        }
        bool isNeutral() const
        {
            return brightness == 0.0 && contrast == 0.0 && saturation == 0.0;
        }
    };

    void resetFilterState();
    void startFilterPreview();
    cv::Rect selectionRect() const;
    // Applies a one-shot transform (whole image or selection only) and
    // pushes the previous state onto the undo stack
    void applyOperation(const std::function<cv::Mat(const cv::Mat &)> &transform);
    // Applies a size-changing transform to the original: the result becomes
    // the new source/displayed image and a base checkpoint is pushed.
    // With preserveViewScale the on-screen magnification is kept (Fit mode
    // is pinned to the previous scale) instead of re-fitting the new canvas.
    void applyGeometryOperation(const std::function<cv::Mat(const cv::Mat &)> &transform,
                                bool preserveViewScale = false);
    bool canUndoAny() const;
    bool canRedoAny() const;

    // Checkpoint of a base-changing geometry operation
    struct GeometryCheckpoint {
        cv::Mat source;
        cv::Mat image;
        QRect selection;
    };

    struct RememberedState {
        cv::Mat source;
        cv::Mat original;
        cv::Mat image;
        QStack<cv::Mat> undo;
        QStack<cv::Mat> redo;
        QStack<GeometryCheckpoint> baseUndo;
        QStack<GeometryCheckpoint> baseRedo;
        QRect selection;
    };

    QString m_path;
    bool m_independentZoom = false;
    AppState::ZoomMode m_localZoomMode = AppState::Fit;
    double m_localZoomFactor = 1.0;
    struct LocalZoom {
        AppState::ZoomMode mode = AppState::Fit;
        double factor = 1.0;
    };
    QHash<QString, LocalZoom> m_localZoomByPath;
    bool m_rememberState = false;
    QHash<QString, RememberedState> m_stateByPath;
    cv::Mat m_sourceImage;  // pristine loaded image; toolbar operations
                            // always transform THIS, never a previous result
    cv::Mat m_image;      // displayed image (original + active filter params)
    cv::Mat m_original;   // current base: filter-preview source and undo anchor
    QPixmap m_displayPixmap;  // paint-side cache, converted from m_image
    Status m_status = NoImage;
    QPointF m_pan;
    QPointF m_lastDragPos;
    QPointF m_lastMousePos;
    QPointF m_mouseImagePos;
    bool m_mouseInsideImage = false;
    int m_mouseGray = -1;
    bool m_dragging = false;
    // Right-button interaction: press starts a candidate; movement beyond
    // the drag threshold turns it into a ROI rubber band, a release
    // without movement requests the context menu
    bool m_roiCandidate = false;
    bool m_roiDragging = false;
    QPointF m_roiPressView;
    QPointF m_roiAnchorImage;
    QRect m_selection;  // image pixel coordinates
    QFutureWatcher<cv::Mat> m_loadWatcher;
    QString m_pendingPath;
    FilterParams m_params;
    QFutureWatcher<cv::Mat> m_filterWatcher;
    // Preview work is throttled: a debounce timer coalesces slider-drag
    // bursts and the dedicated single-worker pool caps the CPU cost
    QTimer m_filterDebounce;
    QThreadPool *m_filterPool = nullptr;
    FilterParams m_pendingParams;
    FilterParams m_runningParams;
    bool m_filterBusy = false;
    QStack<cv::Mat> m_undoStack;
    QStack<cv::Mat> m_redoStack;
    QStack<GeometryCheckpoint> m_baseUndoStack;
    QStack<GeometryCheckpoint> m_baseRedoStack;
};

} // namespace chameleon

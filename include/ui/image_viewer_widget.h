#pragma once

#include <QFutureWatcher>
#include <QPointF>
#include <QPixmap>
#include <QStack>
#include <QThreadPool>
#include <QTimer>
#include <QWidget>

#include <opencv2/core.hpp>

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

    // Maps a point in image pixel coordinates to view coordinates
    QPointF mapImageToView(const QPointF &imagePos) const;

    // "" clears back to the empty state
    void setImage(const QString &path);
    bool saveAs(const QString &path) const;

    // Filter pipeline: params live-preview on a background thread;
    // applyFilter() bakes the preview into the image and pushes the
    // previous state onto the undo stack
    void setFilterParams(double brightness, double contrast, double saturation);
    void applyFilter();
    void undo();
    void redo();
    bool hasActiveFilters() const;

signals:
    void filterPreviewChanged();
    void filterParamsReset();
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);

protected:
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
    void updateMousePosition(const QPointF &viewPos);
    void updateDisplayPixmap();
    void updateImageInfo();

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

    QString m_path;
    cv::Mat m_image;      // displayed image (original + active filter params)
    cv::Mat m_original;   // unfiltered source
    QPixmap m_displayPixmap;  // paint-side cache, converted from m_image
    Status m_status = NoImage;
    QPointF m_pan;
    QPointF m_lastDragPos;
    QPointF m_lastMousePos;
    QPointF m_mouseImagePos;
    bool m_mouseInsideImage = false;
    bool m_dragging = false;
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
};

} // namespace chameleon

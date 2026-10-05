#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

namespace chameleon {

// Central image display: zoom/pan via paintEvent, empty/loading/error states.
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
    double effectiveScale() const;
    QPointF imageTopLeft() const;
    double niceTickInterval() const;
    void clampPan();
    void updateMousePosition(const QPointF &viewPos);

    QString m_path;
    QImage m_image;
    Status m_status = NoImage;
    QPointF m_pan;
    QPointF m_lastDragPos;
    QPointF m_lastMousePos;
    QPointF m_mouseImagePos;
    bool m_mouseInsideImage = false;
    bool m_dragging = false;
};

} // namespace chameleon

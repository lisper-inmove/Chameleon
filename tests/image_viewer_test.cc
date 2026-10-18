#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QWheelEvent>
#include <QSignalSpy>
#include <QThreadPool>
#include <QtTest/QTest>

#include "app/app_state.h"
#include <opencv2/imgproc.hpp>

#include "processing/binary.h"
#include "processing/filter_processor.h"
#include "processing/geometry.h"
#include "ui/image_viewer_widget.h"

using chameleon::AppState;
using chameleon::ImageViewerWidget;

namespace {

bool waitForStatus(ImageViewerWidget *viewer, ImageViewerWidget::Status status,
                   int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (viewer->status() == status)
            return true;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    return viewer->status() == status;
}

// On-screen scale of one image pixel (independent of the zoom mode)
double displayedScale(ImageViewerWidget *viewer)
{
    const QPointF a = viewer->mapImageToView(QPointF(0, 0));
    const QPointF b = viewer->mapImageToView(QPointF(100, 0));
    return (b.x() - a.x()) / 100.0;
}

} // namespace

class ImageViewerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto *s = AppState::instance();
        while (!s->images().isEmpty())
            s->closeImage(0);
        s->setZoomMode(AppState::Fit);
        m_viewer = new ImageViewerWidget;
        m_viewer->resize(800, 600);
        m_viewer->show();
        QCoreApplication::processEvents();
    }

    void TearDown() override
    {
        delete m_viewer;
        m_viewer = nullptr;
    }

    static int countColor(const QImage &img, const std::function<bool(const QColor &)> &match)
    {
        int count = 0;
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                if (match(img.pixelColor(x, y)))
                    ++count;
            }
        }
        return count;
    }

    ImageViewerWidget *m_viewer = nullptr;
};

TEST_F(ImageViewerTest, EmptyStateIsNoImage)
{
    EXPECT_EQ(m_viewer->status(), ImageViewerWidget::NoImage);
    EXPECT_TRUE(m_viewer->imagePath().isEmpty());
}

TEST_F(ImageViewerTest, LoadingValidImageReachesReadyAndSetsInfo)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));
}

TEST_F(ImageViewerTest, LoadingCorruptImageShowsErrorState)
{
    m_viewer->setImage(TEST_DATA_DIR "/not_an_image.png");
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Error));
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize());
}

TEST_F(ImageViewerTest, RendersImagePixels)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    const QImage shot = m_viewer->grab().toImage();
    const int orange = countColor(shot, [](const QColor &c) {
        return c.red() > 200 && c.green() > 120 && c.green() < 190 && c.blue() < 80;
    });
    EXPECT_GT(orange, 100);
}

TEST_F(ImageViewerTest, ImageOriginAlignsWithRulerOriginOnLoad)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    // Image (0,0) sits exactly at the ruler origin, i.e. the canvas
    // top-left corner (left ruler 44px, top ruler 20px)
    const QPointF origin = m_viewer->mapImageToView(QPointF(0, 0));
    EXPECT_NEAR(origin.x(), 44.0, 0.5);
    EXPECT_NEAR(origin.y(), 20.0, 0.5);
}

TEST_F(ImageViewerTest, FitModeAlwaysAnchorsToTheCanvasTopLeft)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Fit);

    // Leaving Fit centres the image (Actual); returning to Fit must put
    // the origin back at the canvas top-left, not centre it again
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();
    const QPointF centered = m_viewer->mapImageToView(QPointF(0, 0));
    EXPECT_GT(centered.x(), 44.0);  // not at the top-left any more

    AppState::instance()->setZoomMode(AppState::Fit);
    QCoreApplication::processEvents();
    const QPointF origin = m_viewer->mapImageToView(QPointF(0, 0));
    EXPECT_NEAR(origin.x(), 44.0, 0.5);
    EXPECT_NEAR(origin.y(), 20.0, 0.5);
}

TEST_F(ImageViewerTest, WheelZoomKeepsCursorImagePointStationary)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);  // scale 1.0

    // Pick an image point and zoom with the cursor exactly on it: the
    // point's view position must not move (layout-independent check)
    const QPointF imagePos(320, 200);
    const QPointF cursor = m_viewer->mapImageToView(imagePos);

    QWheelEvent zoomIn(cursor, cursor, QPoint(), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_viewer, &zoomIn);

    const QPointF after = m_viewer->mapImageToView(imagePos);
    EXPECT_NEAR(after.x(), cursor.x(), 1.0);
    EXPECT_NEAR(after.y(), cursor.y(), 1.0);

    // Zoom back out from the same spot
    QWheelEvent zoomOut(cursor, cursor, QPoint(), QPoint(0, -120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_viewer, &zoomOut);
    const QPointF afterOut = m_viewer->mapImageToView(imagePos);
    EXPECT_NEAR(afterOut.x(), cursor.x(), 1.0);
    EXPECT_NEAR(afterOut.y(), cursor.y(), 1.0);
}

TEST_F(ImageViewerTest, PanIsClampedWhenImageSmallerThanViewport)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();

    // Canvas is 756x580 (800x600 minus 44px left and 20px top ruler strips).
    // Load-time alignment happens at Fit scale (disp 756x472.5):
    // pan = (0, -(580-472.5)/2) = (0, -53.75). Switching to Actual keeps
    // the pan; the x range at Actual is +/- (756-640)/2 = 58.
    QTest::mousePress(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(400, 300));
    QTest::mouseMove(m_viewer, QPoint(2000, 300));
    QTest::mouseRelease(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(2000, 300));
    EXPECT_NEAR(m_viewer->panOffset().x(), 58.0, 0.5);
    EXPECT_NEAR(m_viewer->panOffset().y(), 0.0, 0.5);

    QTest::mousePress(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(400, 300));
    QTest::mouseMove(m_viewer, QPoint(-1200, 300));
    QTest::mouseRelease(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(-1200, 300));
    EXPECT_NEAR(m_viewer->panOffset().x(), -58.0, 0.5);
}

TEST_F(ImageViewerTest, PanIsClampedWhenImageLargerThanViewport)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomFactor(2.0);
    QCoreApplication::processEvents();

    // Canvas 756x580, zoom 2 -> displayed 1280x800: pan range +/- (262, 110)
    QTest::mousePress(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(400, 300));
    QTest::mouseMove(m_viewer, QPoint(2000, 2000));
    QTest::mouseRelease(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(2000, 2000));
    EXPECT_NEAR(m_viewer->panOffset().x(), 262.0, 0.5);
    EXPECT_NEAR(m_viewer->panOffset().y(), 110.0, 0.5);
}

TEST_F(ImageViewerTest, MousePositionTracksImageCoordinates)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();

    // Fit anchoring leaves pan at zero: image top-left at Actual
    // = (102, 110), so view (400, 300) -> image pos (298, 190)
    QTest::mouseMove(m_viewer, QPoint(400, 300));
    EXPECT_TRUE(m_viewer->mouseInsideImage());
    EXPECT_NEAR(m_viewer->mouseImagePos().x(), 298.0, 0.5);
    EXPECT_NEAR(m_viewer->mouseImagePos().y(), 190.0, 0.5);

    QTest::mouseMove(m_viewer, QPoint(10, 10));  // outside the image
    EXPECT_FALSE(m_viewer->mouseInsideImage());
}

TEST_F(ImageViewerTest, RulerTicksAreDrawnAtImageOrigin)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();

    const QImage shot = m_viewer->grab().toImage();
    // Tick color is QColor(150, 150, 150); count it along the top band
    // (x ruler) and the left band (y ruler)
    int topTicks = 0;
    int leftTicks = 0;
    for (int y = 0; y < shot.height(); ++y) {
        for (int x = 0; x < shot.width(); ++x) {
            const QColor c = shot.pixelColor(x, y);
            if (c.red() == 150 && c.green() == 150 && c.blue() == 150) {
                if (y <= 10)
                    ++topTicks;
                if (x <= 10)
                    ++leftTicks;
            }
        }
    }
    EXPECT_GT(topTicks, 40);   // x ticks at image-pixel multiples along the top
    EXPECT_GT(leftTicks, 20);  // y ticks along the left edge
}

TEST_F(ImageViewerTest, LoadingStateShowsBeforeAsyncDecodeCompletes)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    // Loading is set synchronously; the decode completes asynchronously
    EXPECT_EQ(m_viewer->status(), ImageViewerWidget::Loading);
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
}

TEST_F(ImageViewerTest, TinyImageFitScaleIsCappedAtMax)
{
    // A 30x30 image would fit at scale ~19 without a cap; the spec's
    // custom zoom range is 10%-800%, so Fit must not exceed it
    const QString tiny = QDir::tempPath() + "/chameleon_tiny_test.png";
    QImage img(30, 30, QImage::Format_RGB32);
    img.fill(Qt::blue);
    ASSERT_TRUE(img.save(tiny, "PNG"));

    AppState::instance()->openImages({tiny});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Fit);

    // Scale = distance between image points (1,0) and (0,0) in view space
    const double scale = m_viewer->mapImageToView(QPointF(1, 0)).x()
                       - m_viewer->mapImageToView(QPointF(0, 0)).x();
    EXPECT_NEAR(scale, 8.0, 0.01);

    QFile::remove(tiny);
}

TEST_F(ImageViewerTest, HorizontalWheelDoesNotChangeZoom)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();

    // Pure horizontal wheel (trackpad horizontal swipe)
    QWheelEvent horizontal(QPointF(400, 300), QPointF(400, 300), QPoint(), QPoint(120, 0),
                           Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_viewer, &horizontal);
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Actual);
    EXPECT_DOUBLE_EQ(AppState::instance()->zoomFactor(), 1.0);
}

TEST_F(ImageViewerTest, FilterParamsChangeDisplayedPixels)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    QSignalSpy spy(m_viewer, &ImageViewerWidget::filterPreviewChanged);
    m_viewer->setFilterParams(50, 0, 0);
    ASSERT_TRUE(spy.wait(3000));
    EXPECT_NE(m_viewer->grab().toImage(), before);

    // Neutral params reset synchronously (implicit sharing, no thread)
    m_viewer->setFilterParams(0, 0, 0);
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, RapidParamChangesCoalesceIntoOnePreview)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    // The preview computation must run on a dedicated single-worker pool:
    // this bounds the CPU cost even under bursts of parameter changes.
    // (The previous implementation spawned one global-pool task per
    // change, pegging every core - completion signals alone cannot show
    // that because setFuture() detaches the superseded futures.)
    auto *pool = m_viewer->findChild<QThreadPool *>();
    ASSERT_NE(pool, nullptr);
    EXPECT_EQ(pool->maxThreadCount(), 1);

    // Simulate a slider drag: 50 value changes in quick succession;
    // they must coalesce into a single debounced computation
    QSignalSpy spy(m_viewer, &ImageViewerWidget::filterPreviewChanged);
    for (int v = 1; v <= 50; ++v)
        m_viewer->setFilterParams(v, 0, 0);

    ASSERT_TRUE(spy.wait(3000));  // the single debounced preview arrives

    // Give any erroneously launched straggler tasks time to report in
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 300) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(ImageViewerTest, ApplyThenUndoRestoresOriginalPixels)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage original = m_viewer->grab().toImage();

    QSignalSpy spy(m_viewer, &ImageViewerWidget::filterPreviewChanged);
    m_viewer->setFilterParams(80, 0, 0);
    ASSERT_TRUE(spy.wait(3000));
    EXPECT_NE(m_viewer->grab().toImage(), original);

    m_viewer->applyFilter();
    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), original);

    m_viewer->redo();
    EXPECT_NE(m_viewer->grab().toImage(), original);
}

TEST_F(ImageViewerTest, LoadsImageWithChineseFilename)
{
    // OpenCV imread must handle UTF-8 (non-ASCII) Windows paths, since
    // the image pipeline no longer goes through Qt's file APIs
    const QString chinese = QDir::tempPath() + QString::fromUtf8("/测试图片_中文.png");
    QFile::remove(chinese);
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", chinese));

    AppState::instance()->openImages({chinese});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));

    QFile::remove(chinese);
}

TEST_F(ImageViewerTest, RightClickWithoutDragRequestsContextMenu)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    QSignalSpy menuSpy(m_viewer, &ImageViewerWidget::contextMenuRequested);
    QTest::mouseClick(m_viewer, Qt::RightButton, Qt::NoModifier, QPoint(300, 300));

    EXPECT_EQ(menuSpy.count(), 1);
    EXPECT_FALSE(m_viewer->hasSelection());
}

TEST_F(ImageViewerTest, RightDragSelectsRegionInImageCoordinates)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);

    QSignalSpy menuSpy(m_viewer, &ImageViewerWidget::contextMenuRequested);
    QSignalSpy selSpy(m_viewer, &ImageViewerWidget::selectionChanged);

    // Drag (in view space, mapped from image coords) from image (100,100)
    // to image (300,200)
    const QPoint start = m_viewer->mapImageToView(QPointF(100, 100)).toPoint();
    const QPoint end = m_viewer->mapImageToView(QPointF(300, 200)).toPoint();
    QTest::mousePress(m_viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(m_viewer, (start + end) / 2);
    QTest::mouseMove(m_viewer, end);
    QTest::mouseRelease(m_viewer, Qt::RightButton, Qt::NoModifier, end);

    const QRect sel = m_viewer->selection();
    EXPECT_NEAR(sel.x(), 100, 2);
    EXPECT_NEAR(sel.y(), 100, 2);
    EXPECT_NEAR(sel.width(), 200, 3);
    EXPECT_NEAR(sel.height(), 100, 3);
    EXPECT_GE(selSpy.count(), 1);
    EXPECT_EQ(menuSpy.count(), 0);  // a drag must not open the menu
}

TEST_F(ImageViewerTest, RightDragClampsSelectionToImageBounds)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);

    const QPoint start = m_viewer->mapImageToView(QPointF(600, 380)).toPoint();
    QTest::mousePress(m_viewer, Qt::RightButton, Qt::NoModifier, start);
    // Drag far beyond the bottom-right image corner (640x400)
    QTest::mouseMove(m_viewer, QPoint(799, 599));
    QTest::mouseRelease(m_viewer, Qt::RightButton, Qt::NoModifier, QPoint(799, 599));

    const QRect sel = m_viewer->selection();
    EXPECT_NEAR(sel.x(), 600, 2);
    EXPECT_NEAR(sel.y(), 380, 2);
    EXPECT_EQ(sel.right(), 639);   // clamped to the image edge
    EXPECT_EQ(sel.bottom(), 399);
}

TEST_F(ImageViewerTest, SelectAllAndClearSelection)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    QSignalSpy selSpy(m_viewer, &ImageViewerWidget::selectionChanged);

    m_viewer->selectAll();
    EXPECT_EQ(m_viewer->selection(), QRect(0, 0, 640, 400));
    EXPECT_TRUE(m_viewer->hasSelection());
    EXPECT_GE(selSpy.count(), 1);

    m_viewer->clearSelection();
    EXPECT_FALSE(m_viewer->hasSelection());
    EXPECT_GE(selSpy.count(), 2);
}

TEST_F(ImageViewerTest, EscapeClearsSelection)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    // Real-world ESC delivery requires the viewer to take focus on click
    EXPECT_TRUE(m_viewer->focusPolicy() & Qt::ClickFocus);

    m_viewer->selectAll();
    ASSERT_TRUE(m_viewer->hasSelection());

    QSignalSpy selSpy(m_viewer, &ImageViewerWidget::selectionChanged);
    QTest::keyClick(m_viewer, Qt::Key_Escape);

    EXPECT_FALSE(m_viewer->hasSelection());
    EXPECT_GE(selSpy.count(), 1);
}

TEST_F(ImageViewerTest, EscapeWithoutSelectionIsNoOp)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    QSignalSpy selSpy(m_viewer, &ImageViewerWidget::selectionChanged);
    QTest::keyClick(m_viewer, Qt::Key_Escape);

    EXPECT_FALSE(m_viewer->hasSelection());
    EXPECT_EQ(selSpy.count(), 0);
}

TEST_F(ImageViewerTest, SelectionImageCropsCorrectRegion)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);

    // Drag-select the top-left 20x20 corner (pure background: BGR 200,120,30)
    const QPoint start = m_viewer->mapImageToView(QPointF(0, 0)).toPoint();
    const QPoint end = m_viewer->mapImageToView(QPointF(20, 20)).toPoint();
    QTest::mousePress(m_viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(m_viewer, end);
    QTest::mouseRelease(m_viewer, Qt::RightButton, Qt::NoModifier, end);

    const cv::Mat crop = m_viewer->selectionImage();
    ASSERT_FALSE(crop.empty());
    EXPECT_NEAR(crop.cols, 20, 3);
    EXPECT_NEAR(crop.rows, 20, 3);
    const cv::Vec3b px = crop.at<cv::Vec3b>(0, 0);
    EXPECT_NEAR(int(px[0]), 200, 4);  // B
    EXPECT_NEAR(int(px[1]), 120, 4);  // G
    EXPECT_NEAR(int(px[2]), 30, 4);   // R
}

TEST_F(ImageViewerTest, SelectionOutlineIsRendered)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    m_viewer->selectAll();
    const QImage shot = m_viewer->grab().toImage();
    // Amber outline (255,200,0); distinguishes from the orange ellipse
    // (255,165,0) by the green channel
    int amber = 0;
    for (int y = 0; y < shot.height(); ++y) {
        for (int x = 0; x < shot.width(); ++x) {
            const QColor c = shot.pixelColor(x, y);
            if (c.red() > 240 && c.green() > 185 && c.green() < 215 && c.blue() < 60)
                ++amber;
        }
    }
    EXPECT_GT(amber, 100);
}

TEST_F(ImageViewerTest, EqualizeHistogramChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    m_viewer->equalizeHistogram();
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, EqualizeHistogramAppliesToSelectionOnly)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    // Select the 20x20 top-left corner, then equalize
    const QPoint start = m_viewer->mapImageToView(QPointF(0, 0)).toPoint();
    const QPoint end = m_viewer->mapImageToView(QPointF(20, 20)).toPoint();
    QTest::mousePress(m_viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(m_viewer, end);
    QTest::mouseRelease(m_viewer, Qt::RightButton, Qt::NoModifier, end);
    ASSERT_TRUE(m_viewer->hasSelection());

    m_viewer->equalizeHistogram();
    const QImage after = m_viewer->grab().toImage();
    EXPECT_NE(after, before);

    // Inside the selection the pixels changed; image point (10,10) is
    // away from the 2px amber outline
    const QPoint inside = m_viewer->mapImageToView(QPointF(10, 10)).toPoint();
    EXPECT_NE(after.pixelColor(inside), before.pixelColor(inside));
    // Outside the selection nothing changed
    const QPoint outside = m_viewer->mapImageToView(QPointF(300, 300)).toPoint();
    EXPECT_EQ(after.pixelColor(outside), before.pixelColor(outside));
}

TEST_F(ImageViewerTest, EqualizeClaheChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    m_viewer->equalizeClahe();
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, MatchHistogramChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    // A solid bright template remaps the luminance drastically
    const cv::Mat templateImage(32, 32, CV_8UC3, cv::Scalar(200, 200, 200));
    m_viewer->matchHistogram(templateImage);
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, ApplyConvolutionChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    chameleon::BlurParams params;
    params.type = chameleon::BlurType::Gaussian;
    params.kernelSize = 7;
    m_viewer->applyConvolution(params);
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, ApplyGradientChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    chameleon::GradientParams params;
    params.type = chameleon::GradientType::Sobel;
    params.dx = 1;
    params.dy = 0;
    params.ksize = 3;
    m_viewer->applyGradient(params);
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, ApplyEdgeDetectionChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    chameleon::CannyParams params;
    params.threshold1 = 50;
    params.threshold2 = 150;
    m_viewer->applyEdgeDetection(params);
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, AddNoiseChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    chameleon::SaltPepperParams saltPepper;
    saltPepper.proportion = 0.05;
    m_viewer->addSaltPepperNoise(saltPepper);
    EXPECT_NE(m_viewer->grab().toImage(), before);
    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);

    chameleon::GaussianNoiseParams gaussian;
    m_viewer->addGaussianNoise(gaussian);
    EXPECT_NE(m_viewer->grab().toImage(), before);
    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, ApplySharpeningChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    chameleon::SharpenParams params;
    params.ksize = 3;
    params.strength = 1.0;
    m_viewer->applySharpening(params);
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, RememberedStateSurvivesTabSwitch)
{
    const QString pathA = QDir::tempPath() + "/chameleon_tab_a.png";
    const QString pathB = QDir::tempPath() + "/chameleon_tab_b.png";
    QFile::remove(pathA);
    QFile::remove(pathB);
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", pathA));
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", pathB));

    // The AppState image list gates the remembering (a closed image is
    // forgotten)
    AppState::instance()->openImages({pathA, pathB});
    m_viewer->setRememberStatePerPath(true);

    m_viewer->setImage(pathA);
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage pristine = m_viewer->grab().toImage();
    m_viewer->equalizeHistogram();
    const QImage edited = m_viewer->grab().toImage();
    ASSERT_NE(edited, pristine);

    // Switch away and back: the edits are restored synchronously
    m_viewer->setImage(pathB);
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    m_viewer->setImage(pathA);
    EXPECT_EQ(m_viewer->status(), ImageViewerWidget::Ready);  // no reload
    EXPECT_EQ(m_viewer->grab().toImage(), edited);

    // The undo stack survived too
    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), pristine);

    // Closing an image forgets its remembered edits
    AppState::instance()->closeImage(0);  // removes pathA
    m_viewer->retainRememberedStates(AppState::instance()->images());
    m_viewer->setImage(pathA);
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(m_viewer->grab().toImage(), pristine);  // fresh decode

    while (!AppState::instance()->images().isEmpty())
        AppState::instance()->closeImage(0);
    QFile::remove(pathA);
    QFile::remove(pathB);
}

TEST_F(ImageViewerTest, ReloadImageDiscardsProcessingEdits)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setRememberStatePerPath(true);
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage pristine = m_viewer->grab().toImage();

    m_viewer->equalizeHistogram();
    ASSERT_NE(m_viewer->grab().toImage(), pristine);

    m_viewer->reloadImage();
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(m_viewer->grab().toImage(), pristine);
}

TEST_F(ImageViewerTest, ApplyThresholdChangesImageAndUndoRestores)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage before = m_viewer->grab().toImage();

    chameleon::ThresholdParams params;
    params.thresh = 128;
    params.maxval = 255;
    params.type = chameleon::ThresholdType::Binary;
    m_viewer->applyThreshold(params);
    EXPECT_NE(m_viewer->grab().toImage(), before);

    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), before);
}

TEST_F(ImageViewerTest, ToolbarOperationsAlwaysStartFromTheOriginal)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    // A second viewer gets the same image and thresholds it directly
    ImageViewerWidget other;
    other.resize(800, 600);
    other.show();
    other.setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(&other, ImageViewerWidget::Ready));
    QCoreApplication::processEvents();

    chameleon::ThresholdParams threshold;
    threshold.thresh = 128;
    other.applyThreshold(threshold);
    const QImage direct = other.grab().toImage();

    // The main viewer blurs first, then thresholds: the blur must be
    // discarded because toolbar operations always start from the original
    chameleon::BlurParams blur;
    blur.type = chameleon::BlurType::Gaussian;
    blur.kernelSize = 7;
    m_viewer->applyConvolution(blur);
    const QImage blurred = m_viewer->grab().toImage();
    m_viewer->applyThreshold(threshold);
    EXPECT_EQ(m_viewer->grab().toImage(), direct);

    // Undo brings the blurred state back for comparison
    m_viewer->undo();
    EXPECT_EQ(m_viewer->grab().toImage(), blurred);
}

TEST_F(ImageViewerTest, SelectionOperationStartsFromTheOriginal)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const QImage pristine = m_viewer->grab().toImage();

    chameleon::BlurParams blur;
    blur.type = chameleon::BlurType::Gaussian;
    blur.kernelSize = 7;
    m_viewer->applyConvolution(blur);
    ASSERT_NE(m_viewer->grab().toImage(), pristine);

    // Threshold only the 20x20 top-left corner: the rest of the image
    // must show the ORIGINAL (the blur is discarded)
    const QPoint start = m_viewer->mapImageToView(QPointF(0, 0)).toPoint();
    const QPoint end = m_viewer->mapImageToView(QPointF(20, 20)).toPoint();
    QTest::mousePress(m_viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(m_viewer, end);
    QTest::mouseRelease(m_viewer, Qt::RightButton, Qt::NoModifier, end);
    ASSERT_TRUE(m_viewer->hasSelection());

    chameleon::ThresholdParams threshold;
    threshold.thresh = 128;
    m_viewer->applyThreshold(threshold);
    const QImage after = m_viewer->grab().toImage();

    // (50, 70) sits on the lime rectangle's border, where the blur
    // visibly mixed colors: the original value must show through
    const QPoint outside = m_viewer->mapImageToView(QPointF(50, 70)).toPoint();
    EXPECT_EQ(after.pixelColor(outside), pristine.pixelColor(outside));
    const QPoint inside = m_viewer->mapImageToView(QPointF(10, 10)).toPoint();
    EXPECT_NE(after.pixelColor(inside), pristine.pixelColor(inside));
}

TEST_F(ImageViewerTest, OperationsUseTheOriginalAfterTabSwitch)
{
    const QString pathA = QDir::tempPath() + "/chameleon_orig_a.png";
    const QString pathB = QDir::tempPath() + "/chameleon_orig_b.png";
    QFile::remove(pathA);
    QFile::remove(pathB);
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", pathA));
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", pathB));

    AppState::instance()->openImages({pathA, pathB});
    m_viewer->setRememberStatePerPath(true);

    m_viewer->setImage(pathA);
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    chameleon::BlurParams blur;
    blur.type = chameleon::BlurType::Gaussian;
    blur.kernelSize = 7;
    m_viewer->applyConvolution(blur);
    const QImage blurredOnce = m_viewer->grab().toImage();

    m_viewer->setImage(pathB);
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    m_viewer->setImage(pathA);
    EXPECT_EQ(m_viewer->grab().toImage(), blurredOnce);

    // Applying the same blur again starts from the original, so the
    // result is identical (not a doubly-blurred image)
    m_viewer->applyConvolution(blur);
    EXPECT_EQ(m_viewer->grab().toImage(), blurredOnce);

    while (!AppState::instance()->images().isEmpty())
        AppState::instance()->closeImage(0);
    QFile::remove(pathA);
    QFile::remove(pathB);
}

TEST_F(ImageViewerTest, MouseGrayValueTracksThePixel)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Actual);

    // The test image background is BGR (200, 120, 30)
    const cv::Mat pixel(1, 1, CV_8UC3, cv::Scalar(200, 120, 30));
    cv::Mat grayPixel;
    cv::cvtColor(pixel, grayPixel, cv::COLOR_BGR2GRAY);
    const int expected = grayPixel.at<uchar>(0, 0);

    QTest::mouseMove(m_viewer, m_viewer->mapImageToView(QPointF(10, 10)).toPoint());
    EXPECT_TRUE(m_viewer->mouseInsideImage());
    EXPECT_EQ(m_viewer->mouseGrayValue(), expected);

    QTest::mouseMove(m_viewer, QPoint(800, 590));  // outside the image
    EXPECT_FALSE(m_viewer->mouseInsideImage());
}

TEST_F(ImageViewerTest, CropToSelectionBecomesTheNewBaseAndUndoes)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    const QPoint start = m_viewer->mapImageToView(QPointF(0, 0)).toPoint();
    const QPoint end = m_viewer->mapImageToView(QPointF(20, 20)).toPoint();
    QTest::mousePress(m_viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(m_viewer, end);
    QTest::mouseRelease(m_viewer, Qt::RightButton, Qt::NoModifier, end);
    ASSERT_TRUE(m_viewer->hasSelection());

    chameleon::GeometryParams params;
    params.operation = chameleon::GeometryOperation::Crop;
    m_viewer->applyGeometry(params);
    EXPECT_NEAR(AppState::instance()->imageInfo().width(), 20, 2);
    EXPECT_NEAR(AppState::instance()->imageInfo().height(), 20, 2);
    EXPECT_FALSE(m_viewer->hasSelection());

    // Undo restores the previous base
    m_viewer->undo();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));
}

TEST_F(ImageViewerTest, GeometryRotationChangesSizeAndUndoes)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    chameleon::GeometryParams params;
    params.operation = chameleon::GeometryOperation::Rotate;
    params.angle = 90;
    m_viewer->applyGeometry(params);
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(400, 640));

    m_viewer->undo();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));
    m_viewer->redo();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(400, 640));
}

TEST_F(ImageViewerTest, Rotate90ThenMinus90RestoresTheOriginalImage)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    const cv::Mat original = m_viewer->currentImage().clone();

    chameleon::GeometryParams params;
    params.operation = chameleon::GeometryOperation::Rotate;
    params.angle = 90;
    m_viewer->applyGeometry(params);
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(400, 640));

    params.angle = -90;
    m_viewer->applyGeometry(params);
    ASSERT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));
    const cv::Mat restored = m_viewer->currentImage();
    ASSERT_EQ(restored.size(), original.size());
    cv::Mat diff;
    cv::absdiff(restored, original, diff);
    EXPECT_EQ(cv::countNonZero(diff.reshape(1)), 0);
}

TEST_F(ImageViewerTest, RotationKeepsTheDisplayScale)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));
    AppState::instance()->setZoomMode(AppState::Fit);
    const double before = displayedScale(m_viewer);
    EXPECT_GT(before, 0.0);

    chameleon::GeometryParams params;
    params.operation = chameleon::GeometryOperation::Rotate;
    params.angle = 90;
    m_viewer->applyGeometry(params);

    // The canvas is 400x640 now: re-fitting to the window would shrink the
    // image, so rotation must pin the previously displayed scale instead
    ASSERT_EQ(AppState::instance()->imageInfo(), QSize(400, 640));
    EXPECT_NEAR(displayedScale(m_viewer), before, 1e-6);
}

TEST_F(ImageViewerTest, SaveAsWritesPng)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_TRUE(waitForStatus(m_viewer, ImageViewerWidget::Ready));

    const QString out = QDir::tempPath() + "/chameleon_save_test.png";
    QFile::remove(out);
    EXPECT_TRUE(m_viewer->saveAs(out));
    EXPECT_TRUE(QFileInfo::exists(out));
    QFile::remove(out);
}

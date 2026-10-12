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
    EXPECT_NEAR(m_viewer->panOffset().y(), -53.75, 0.5);

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

    // Pan kept from load-time Fit alignment: image top-left at Actual
    // = (102, 56.25), so view (400, 300) -> image pos (298, 243.75)
    QTest::mouseMove(m_viewer, QPoint(400, 300));
    EXPECT_TRUE(m_viewer->mouseInsideImage());
    EXPECT_NEAR(m_viewer->mouseImagePos().x(), 298.0, 0.5);
    EXPECT_NEAR(m_viewer->mouseImagePos().y(), 243.75, 0.5);

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

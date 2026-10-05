#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QWheelEvent>
#include <QtTest/QTest>

#include "app/app_state.h"
#include "ui/image_viewer_widget.h"

using chameleon::AppState;
using chameleon::ImageViewerWidget;

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
    EXPECT_EQ(m_viewer->status(), ImageViewerWidget::Ready);
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));
}

TEST_F(ImageViewerTest, LoadingCorruptImageShowsErrorState)
{
    m_viewer->setImage(TEST_DATA_DIR "/not_an_image.png");
    EXPECT_EQ(m_viewer->status(), ImageViewerWidget::Error);
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize());
}

TEST_F(ImageViewerTest, RendersImagePixels)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);

    const QImage shot = m_viewer->grab().toImage();
    const int orange = countColor(shot, [](const QColor &c) {
        return c.red() > 200 && c.green() > 120 && c.green() < 190 && c.blue() < 80;
    });
    EXPECT_GT(orange, 100);
}

TEST_F(ImageViewerTest, WheelZoomKeepsCursorImagePointStationary)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);
    AppState::instance()->setZoomMode(AppState::Actual);  // scale 1.0

    // Image 640x400 at scale 1 in an 800x600 viewport -> top-left (80, 100)
    const QPoint cursor(400, 300);
    const QPointF imagePos(320, 200);
    const QPointF before = m_viewer->mapImageToView(imagePos);
    ASSERT_NEAR(before.x(), cursor.x(), 0.5);
    ASSERT_NEAR(before.y(), cursor.y(), 0.5);

    // Zoom in with the cursor at (400, 300): the image point under the
    // cursor must stay under the cursor
    QWheelEvent zoomIn(QPointF(cursor), QPointF(cursor), QPoint(), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_viewer, &zoomIn);

    const QPointF after = m_viewer->mapImageToView(imagePos);
    EXPECT_NEAR(after.x(), cursor.x(), 1.0);
    EXPECT_NEAR(after.y(), cursor.y(), 1.0);

    // Zoom back out from the same spot
    QWheelEvent zoomOut(QPointF(cursor), QPointF(cursor), QPoint(), QPoint(0, -120),
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
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();

    // Displayed 640x400 in 800x600 viewport: pan range is +/- (800-640)/2 = 80
    QTest::mousePress(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(400, 300));
    QTest::mouseMove(m_viewer, QPoint(2000, 300));
    QTest::mouseRelease(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(2000, 300));
    EXPECT_NEAR(m_viewer->panOffset().x(), 80.0, 0.5);
    EXPECT_NEAR(m_viewer->panOffset().y(), 0.0, 0.5);

    QTest::mousePress(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(400, 300));
    QTest::mouseMove(m_viewer, QPoint(-1200, 300));
    QTest::mouseRelease(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(-1200, 300));
    EXPECT_NEAR(m_viewer->panOffset().x(), -80.0, 0.5);
}

TEST_F(ImageViewerTest, PanIsClampedWhenImageLargerThanViewport)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);
    AppState::instance()->setZoomFactor(2.0);
    QCoreApplication::processEvents();

    // Displayed 1280x800 in 800x600 viewport: pan range +/- (240, 100)
    QTest::mousePress(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(400, 300));
    QTest::mouseMove(m_viewer, QPoint(2000, 2000));
    QTest::mouseRelease(m_viewer, Qt::LeftButton, Qt::NoModifier, QPoint(2000, 2000));
    EXPECT_NEAR(m_viewer->panOffset().x(), 240.0, 0.5);
    EXPECT_NEAR(m_viewer->panOffset().y(), 100.0, 0.5);
}

TEST_F(ImageViewerTest, MousePositionTracksImageCoordinates)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);
    AppState::instance()->setZoomMode(AppState::Actual);
    QCoreApplication::processEvents();

    // Image 640x400 at scale 1 -> top-left (80, 100)
    QTest::mouseMove(m_viewer, QPoint(400, 300));  // image pos (320, 200)
    EXPECT_TRUE(m_viewer->mouseInsideImage());
    EXPECT_NEAR(m_viewer->mouseImagePos().x(), 320.0, 0.5);
    EXPECT_NEAR(m_viewer->mouseImagePos().y(), 200.0, 0.5);

    QTest::mouseMove(m_viewer, QPoint(10, 10));  // outside the image
    EXPECT_FALSE(m_viewer->mouseInsideImage());
}

TEST_F(ImageViewerTest, RulerTicksAreDrawnAtImageOrigin)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);
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

TEST_F(ImageViewerTest, SaveAsWritesPng)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    m_viewer->setImage(AppState::instance()->currentImage());
    ASSERT_EQ(m_viewer->status(), ImageViewerWidget::Ready);

    const QString out = QDir::tempPath() + "/chameleon_save_test.png";
    QFile::remove(out);
    EXPECT_TRUE(m_viewer->saveAs(out));
    EXPECT_TRUE(QFileInfo::exists(out));
    QFile::remove(out);
}

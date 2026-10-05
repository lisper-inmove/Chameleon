#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

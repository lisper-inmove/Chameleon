#include <gtest/gtest.h>

#include "app/app_state.h"

using chameleon::AppState;

class AppStateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto *s = AppState::instance();
        while (!s->images().isEmpty())
            s->closeImage(0);
        s->setZoomMode(AppState::Fit);
    }
};

TEST_F(AppStateTest, OpenImagesAppendsAndSelectsFirstNew)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png", "F:/b.png"});
    EXPECT_EQ(s->images().size(), 2);
    EXPECT_EQ(s->currentIndex(), 0);
    EXPECT_TRUE(s->hasImage());
    EXPECT_EQ(s->currentImage().toStdString(), "F:/a.png");

    s->openImages({"F:/c.png"});
    EXPECT_EQ(s->images().size(), 3);
    EXPECT_EQ(s->currentIndex(), 2);
}

TEST_F(AppStateTest, OpenImagesIgnoresEmptyList)
{
    auto *s = AppState::instance();
    s->openImages({});
    EXPECT_TRUE(s->images().isEmpty());
    EXPECT_FALSE(s->hasImage());
}

TEST_F(AppStateTest, CloseMiddleKeepsIndexValid)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png", "F:/b.png", "F:/c.png"});
    s->setCurrentIndex(2);
    s->closeImage(1);
    EXPECT_EQ(s->images().size(), 2);
    EXPECT_EQ(s->currentIndex(), 1);
    EXPECT_EQ(s->currentImage().toStdString(), "F:/c.png");
}

TEST_F(AppStateTest, CloseCurrentLastClampsIndex)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png", "F:/b.png"});
    s->closeImage(1);
    EXPECT_EQ(s->currentIndex(), 0);
    EXPECT_TRUE(s->hasImage());
}

TEST_F(AppStateTest, CloseAllResetsState)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png"});
    s->closeImage(0);
    EXPECT_FALSE(s->hasImage());
    EXPECT_EQ(s->currentIndex(), -1);
    EXPECT_TRUE(s->currentImage().isEmpty());
}

TEST_F(AppStateTest, ZoomFactorIsClamped)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png"});
    s->setZoomFactor(42.0);
    EXPECT_DOUBLE_EQ(s->zoomFactor(), 8.0);
    EXPECT_EQ(s->zoomMode(), AppState::Custom);
    s->setZoomFactor(0.01);
    EXPECT_DOUBLE_EQ(s->zoomFactor(), 0.1);
}

TEST_F(AppStateTest, ZoomIsPerImage)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png", "F:/b.png"});
    s->setZoomFactor(2.0);
    s->setCurrentIndex(1);
    EXPECT_DOUBLE_EQ(s->zoomFactorForCurrent(), 1.0);
    s->setZoomFactor(3.0);
    s->setCurrentIndex(0);
    EXPECT_DOUBLE_EQ(s->zoomFactorForCurrent(), 2.0);
}

TEST_F(AppStateTest, RecentFilesDedupeAndCapAtEight)
{
    auto *s = AppState::instance();
    QStringList many;
    for (int i = 0; i < 10; ++i)
        many << QString("F:/img%1.png").arg(i);
    s->openImages(many);
    EXPECT_EQ(s->recentFiles().size(), 8);
    EXPECT_EQ(s->recentFiles().first().toStdString(), "F:/img9.png");

    s->openImages({"F:/img0.png"});
    EXPECT_EQ(s->recentFiles().size(), 8);
    EXPECT_EQ(s->recentFiles().first().toStdString(), "F:/img0.png");
}

TEST_F(AppStateTest, SetCurrentIndexClamps)
{
    auto *s = AppState::instance();
    s->openImages({"F:/a.png", "F:/b.png"});
    s->setCurrentIndex(5);
    EXPECT_EQ(s->currentIndex(), 1);
    s->setCurrentIndex(-3);
    EXPECT_EQ(s->currentIndex(), 0);
}

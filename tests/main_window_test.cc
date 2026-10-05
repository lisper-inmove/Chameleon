#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QStatusBar>
#include <QtTest/QTest>

#include "app/app_state.h"
#include "ui/image_viewer_widget.h"
#include "ui/main_window.h"

using chameleon::AppState;
using chameleon::ImageViewerWidget;
using chameleon::MainWindow;

class MainWindowTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        s_window = new MainWindow;
        s_window->resize(1200, 800);
        s_window->show();
        QCoreApplication::processEvents();
    }

    static void TearDownTestSuite()
    {
        delete s_window;
        s_window = nullptr;
    }

    void SetUp() override
    {
        auto *s = AppState::instance();
        while (!s->images().isEmpty())
            s->closeImage(0);
        s->setZoomMode(AppState::Fit);
        QCoreApplication::processEvents();
    }

    QLabel *label(const char *name)
    {
        return s_window->findChild<QLabel *>(name);
    }

    QAction *action(const char *name)
    {
        return s_window->findChild<QAction *>(name);
    }

    static MainWindow *s_window;
};

MainWindow *MainWindowTest::s_window = nullptr;

TEST_F(MainWindowTest, HasMenusToolbarDocksAndViewer)
{
    const auto menus = s_window->menuBar()->findChildren<QMenu *>();
    QStringList titles;
    for (auto *m : menus)
        titles << m->title();
    EXPECT_TRUE(titles.contains("文件(&F)"));
    EXPECT_TRUE(titles.contains("视图(&V)"));
    EXPECT_TRUE(titles.contains("帮助(&H)"));

    EXPECT_NE(s_window->findChild<QListWidget *>("thumbnailList"), nullptr);
    EXPECT_NE(s_window->findChild<QDockWidget *>("leftDock"), nullptr);
    EXPECT_NE(s_window->findChild<QDockWidget *>("rightDock"), nullptr);
    EXPECT_NE(s_window->findChild<ImageViewerWidget *>("imageViewer"), nullptr);
    EXPECT_NE(action("openAction"), nullptr);
    EXPECT_NE(action("saveAction"), nullptr);
    EXPECT_NE(action("fitAction"), nullptr);
    EXPECT_NE(action("actualAction"), nullptr);
}

TEST_F(MainWindowTest, EmptyStateShowsHintAndDisablesActions)
{
    auto *fn = label("statusFileName");
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->text().toStdString(), "未打开图片");

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    EXPECT_EQ(viewer->status(), ImageViewerWidget::NoImage);

    EXPECT_FALSE(action("saveAction")->isEnabled());
    EXPECT_FALSE(action("closeAction")->isEnabled());
    EXPECT_FALSE(action("fitAction")->isEnabled());
    EXPECT_FALSE(action("actualAction")->isEnabled());
}

TEST_F(MainWindowTest, OpenImageUpdatesStatusBarAndEnablesActions)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    EXPECT_EQ(viewer->status(), ImageViewerWidget::Ready);

    EXPECT_EQ(label("statusFileName")->text().toStdString(), "test_image.png");
    EXPECT_EQ(label("statusSize")->text().toStdString(), "640 × 400");
    EXPECT_EQ(label("statusFormat")->text().toStdString(), "PNG");
    EXPECT_TRUE(action("saveAction")->isEnabled());
    EXPECT_TRUE(action("fitAction")->isEnabled());
}

TEST_F(MainWindowTest, ClosingAllImagesReturnsToEmptyState)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    AppState::instance()->closeImage(0);
    QCoreApplication::processEvents();

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    EXPECT_EQ(viewer->status(), ImageViewerWidget::NoImage);
    EXPECT_EQ(label("statusFileName")->text().toStdString(), "未打开图片");
    EXPECT_FALSE(action("saveAction")->isEnabled());
}

TEST_F(MainWindowTest, ViewerRendersImagePixels)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    const QImage shot = viewer->grab().toImage();
    int orange = 0;
    for (int y = 0; y < shot.height(); ++y) {
        for (int x = 0; x < shot.width(); ++x) {
            const QColor c = shot.pixelColor(x, y);
            if (c.red() > 200 && c.green() > 120 && c.green() < 190 && c.blue() < 80)
                ++orange;
        }
    }
    EXPECT_GT(orange, 100);
}

TEST_F(MainWindowTest, DarkThemeChangesPalette)
{
    AppState::instance()->setTheme("dark");
    QCoreApplication::processEvents();
    EXPECT_LT(QApplication::palette().color(QPalette::Window).lightness(), 100);
    AppState::instance()->setTheme("system");
}

TEST_F(MainWindowTest, OpeningMoreImagesKeepsNewSelection)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    AppState::instance()->openImages({TEST_DATA_DIR "/not_an_image.png"});
    QCoreApplication::processEvents();

    // Must select the newly opened image, not clamp back to the first
    EXPECT_EQ(AppState::instance()->currentIndex(), 1);
    auto *list = s_window->findChild<QListWidget *>("thumbnailList");
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->currentRow(), 1);

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    EXPECT_EQ(viewer->status(), ImageViewerWidget::Error);  // the corrupt new image
}

TEST_F(MainWindowTest, ClosingCurrentTabShowsNeighborNotFirst)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    AppState::instance()->openImages({TEST_DATA_DIR "/not_an_image.png"});
    QCoreApplication::processEvents();
    ASSERT_EQ(AppState::instance()->currentIndex(), 1);

    AppState::instance()->closeImage(1);
    QCoreApplication::processEvents();

    EXPECT_EQ(AppState::instance()->currentIndex(), 0);
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    EXPECT_EQ(viewer->status(), ImageViewerWidget::Ready);
    EXPECT_EQ(viewer->imagePath().toStdString(),
              std::string(TEST_DATA_DIR "/test_image.png"));
}

TEST_F(MainWindowTest, ReloadResetsZoomAndKeepsImage)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_EQ(viewer->status(), ImageViewerWidget::Ready);

    // Zoom in and pan away from center
    AppState::instance()->setZoomFactor(2.0);
    QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
    QTest::mouseMove(viewer, QPoint(260, 240));
    QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(260, 240));
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Custom);

    auto *reload = action("reloadAction");
    ASSERT_NE(reload, nullptr);
    reload->trigger();
    QCoreApplication::processEvents();

    // Back to the initial state: Fit zoom, pan reset, image still shown
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Fit);
    EXPECT_EQ(viewer->panOffset(), QPointF());
    EXPECT_EQ(viewer->status(), ImageViewerWidget::Ready);
    EXPECT_EQ(viewer->imagePath().toStdString(),
              std::string(TEST_DATA_DIR "/test_image.png"));
}

TEST_F(MainWindowTest, ClosingBackgroundTabKeepsCurrentImageAndPan)
{
    AppState::instance()->openImages(
        {TEST_DATA_DIR "/test_image.png", TEST_DATA_DIR "/not_an_image.png"});
    AppState::instance()->setCurrentIndex(0);  // watch the valid image
    QCoreApplication::processEvents();

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_EQ(viewer->status(), ImageViewerWidget::Ready);

    // Drag to pan the current image
    QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseMove(viewer, QPoint(150, 120));
    QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(150, 120));
    const QPointF panBefore = viewer->panOffset();
    EXPECT_GT(qAbs(panBefore.x()), 0.1);

    AppState::instance()->closeImage(1);  // close the background tab
    QCoreApplication::processEvents();

    EXPECT_EQ(AppState::instance()->currentIndex(), 0);
    EXPECT_EQ(viewer->status(), ImageViewerWidget::Ready);
    EXPECT_EQ(viewer->imagePath().toStdString(),
              std::string(TEST_DATA_DIR "/test_image.png"));
    // The current image must not be reloaded (pan position preserved)
    EXPECT_EQ(viewer->panOffset(), panBefore);
}

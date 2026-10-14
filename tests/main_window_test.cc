#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QStatusBar>
#include <QtTest/QTest>

#include "app/app_state.h"
#include "processing/image_io.h"
#include "ui/histogram_window.h"
#include "ui/image_viewer_widget.h"
#include "ui/main_window.h"

using chameleon::AppState;
using chameleon::HistogramWindow;
using chameleon::ImageViewerWidget;
using chameleon::MainWindow;

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
    EXPECT_TRUE(titles.contains("编辑(&E)"));
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
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

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
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
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

TEST_F(MainWindowTest, DockCloseSyncsMenuToggle)
{
    auto *leftDock = s_window->findChild<QDockWidget *>("leftDock");
    auto *toggle = action("leftDockToggle");
    ASSERT_NE(leftDock, nullptr);
    ASSERT_NE(toggle, nullptr);
    EXPECT_TRUE(toggle->isChecked());

    leftDock->close();  // like clicking the dock title-bar X
    QCoreApplication::processEvents();
    EXPECT_FALSE(leftDock->isVisible());
    EXPECT_FALSE(toggle->isChecked());

    toggle->trigger();
    QCoreApplication::processEvents();
    EXPECT_TRUE(leftDock->isVisible());
    EXPECT_TRUE(toggle->isChecked());
}

TEST_F(MainWindowTest, FilterPipelineWiresUpThroughPanelAndEditMenu)
{
    EXPECT_FALSE(action("undoAction")->isEnabled());
    EXPECT_FALSE(action("redoAction")->isEnabled());

    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
    const QImage before = viewer->grab().toImage();

    auto *brightness = s_window->findChild<QSlider *>("brightnessSlider");
    ASSERT_NE(brightness, nullptr);
    EXPECT_TRUE(brightness->isEnabled());

    QSignalSpy spy(viewer, &ImageViewerWidget::filterPreviewChanged);
    brightness->setValue(50);
    ASSERT_TRUE(spy.wait(3000));
    EXPECT_NE(viewer->grab().toImage(), before);

    auto *apply = s_window->findChild<QPushButton *>("applyButton");
    ASSERT_NE(apply, nullptr);
    apply->click();
    EXPECT_TRUE(action("undoAction")->isEnabled());
    EXPECT_EQ(brightness->value(), 0);  // sliders reset after apply

    action("undoAction")->trigger();
    EXPECT_EQ(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("redoAction")->isEnabled());
    EXPECT_FALSE(action("undoAction")->isEnabled());
}

TEST_F(MainWindowTest, ThumbnailsRenderViaOpenCv)
{
    // Chinese filename exercises the OpenCV UTF-8 loader in the thumbnail
    // path (thumbnails are decoded by OpenCV, not Qt image plugins)
    const QString chinese = QDir::tempPath() + QString::fromUtf8("/缩略图_测试.png");
    QFile::remove(chinese);
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", chinese));

    AppState::instance()->openImages({chinese});
    QCoreApplication::processEvents();

    auto *list = s_window->findChild<QListWidget *>("thumbnailList");
    ASSERT_NE(list, nullptr);
    ASSERT_EQ(list->count(), 1);
    QWidget *itemWidget = list->itemWidget(list->item(0));
    ASSERT_NE(itemWidget, nullptr);

    bool foundPixmap = false;
    const auto labels = itemWidget->findChildren<QLabel *>();
    for (auto *label : labels) {
        if (!label->pixmap().isNull())
            foundPixmap = true;
    }
    EXPECT_TRUE(foundPixmap);

    QFile::remove(chinese);
}

TEST_F(MainWindowTest, ViewerContextMenuActions)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    QScopedPointer<QMenu> menu(s_window->createViewerContextMenu());
    ASSERT_NE(menu.data(), nullptr);
    QStringList texts;
    for (auto *action : menu->actions()) {
        if (!action->isSeparator())
            texts << action->text();
    }
    EXPECT_TRUE(texts.contains("适应窗口"));
    EXPECT_TRUE(texts.contains("实际大小"));
    EXPECT_TRUE(texts.contains("全选"));
    EXPECT_TRUE(texts.contains("清除选区"));
    EXPECT_TRUE(texts.contains("保存选区为 PNG..."));

    // 全选 selects the whole image through the menu action
    for (auto *action : menu->actions()) {
        if (action->text() == "全选") {
            action->trigger();
            break;
        }
    }
    EXPECT_TRUE(viewer->hasSelection());
    EXPECT_EQ(viewer->selection(), QRect(0, 0, 640, 400));
}

TEST_F(MainWindowTest, HistogramWindowsOpenPerChannel)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    // Trigger 灰度直方图 from the context menu
    {
        QScopedPointer<QMenu> menu(s_window->createViewerContextMenu());
        bool triggered = false;
        for (auto *action : menu->actions()) {
            if (action->text() == "灰度直方图") {
                action->trigger();
                triggered = true;
            }
        }
        EXPECT_TRUE(triggered);
    }
    QCoreApplication::processEvents();

    // Color image: one popup window per channel (B, G, R)
    auto *blue = s_window->findChild<HistogramWindow *>("histogramWindow_blue");
    auto *green = s_window->findChild<HistogramWindow *>("histogramWindow_green");
    auto *red = s_window->findChild<HistogramWindow *>("histogramWindow_red");
    ASSERT_NE(blue, nullptr);
    ASSERT_NE(green, nullptr);
    ASSERT_NE(red, nullptr);
    EXPECT_EQ(s_window->findChildren<HistogramWindow *>().size(), 3);
    // The blue background (B=200) dominates the whole-image blue histogram
    EXPECT_GT(blue->bins()[200], blue->bins()[0]);

    // The window renders bars
    blue->resize(340, 240);
    blue->show();
    QCoreApplication::processEvents();
    const QImage shot = blue->grab().toImage();
    int barPixels = 0;
    for (int y = 0; y < shot.height(); ++y) {
        for (int x = 0; x < shot.width(); ++x) {
            const QColor c = shot.pixelColor(x, y);
            if (c.blue() > 200 && c.red() < 140 && c.green() < 180)
                ++barPixels;
        }
    }
    EXPECT_GT(barPixels, 20);

    // Replace the batch, then compute from a selection instead: the
    // top-left 20x20 corner is pure background (B=200, G=120, R=30)
    for (auto *w : s_window->findChildren<HistogramWindow *>())
        w->close();
    QCoreApplication::processEvents();

    const QPoint start = viewer->mapImageToView(QPointF(0, 0)).toPoint();
    const QPoint end = viewer->mapImageToView(QPointF(20, 20)).toPoint();
    QTest::mousePress(viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(viewer, end);
    QTest::mouseRelease(viewer, Qt::RightButton, Qt::NoModifier, end);
    ASSERT_TRUE(viewer->hasSelection());

    {
        QScopedPointer<QMenu> menu(s_window->createViewerContextMenu());
        for (auto *action : menu->actions()) {
            if (action->text() == "灰度直方图")
                action->trigger();
        }
    }
    QCoreApplication::processEvents();

    auto *selBlue = s_window->findChild<HistogramWindow *>("histogramWindow_blue");
    auto *selGreen = s_window->findChild<HistogramWindow *>("histogramWindow_green");
    auto *selRed = s_window->findChild<HistogramWindow *>("histogramWindow_red");
    ASSERT_NE(selBlue, nullptr);
    ASSERT_NE(selGreen, nullptr);
    ASSERT_NE(selRed, nullptr);
    EXPECT_DOUBLE_EQ(selBlue->bins()[200], 400);   // 20x20 uniform background
    EXPECT_DOUBLE_EQ(selGreen->bins()[120], 400);
    EXPECT_DOUBLE_EQ(selRed->bins()[30], 400);

    for (auto *w : s_window->findChildren<HistogramWindow *>())
        w->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, GrayImageOpensSingleHistogramWindow)
{
    // A single-channel source gets exactly one window
    const QString path = QDir::tempPath() + "/chameleon_gray_hist.png";
    QFile::remove(path);
    const cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(100));
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(path.toStdString(), gray));

    AppState::instance()->openImages({path});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    QScopedPointer<QMenu> menu(s_window->createViewerContextMenu());
    for (auto *action : menu->actions()) {
        if (action->text() == "灰度直方图")
            action->trigger();
    }
    QCoreApplication::processEvents();

    auto *grayWindow = s_window->findChild<HistogramWindow *>("histogramWindow_gray");
    ASSERT_NE(grayWindow, nullptr);
    EXPECT_EQ(s_window->findChildren<HistogramWindow *>().size(), 1);
    EXPECT_DOUBLE_EQ(grayWindow->bins()[100], 64 * 64);

    for (auto *w : s_window->findChildren<HistogramWindow *>())
        w->close();
    QCoreApplication::processEvents();
    QFile::remove(path);
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
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Error));  // the corrupt new image
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
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(viewer->imagePath().toStdString(),
              std::string(TEST_DATA_DIR "/test_image.png"));
}

TEST_F(MainWindowTest, ReloadResetsZoomAndKeepsImage)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();

    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    // Zoom in and pan away from center
    AppState::instance()->setZoomFactor(2.0);
    QTest::mousePress(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
    QTest::mouseMove(viewer, QPoint(260, 240));
    QTest::mouseRelease(viewer, Qt::LeftButton, Qt::NoModifier, QPoint(260, 240));
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Custom);

    auto *reload = action("reloadAction");
    ASSERT_NE(reload, nullptr);
    reload->trigger();
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    // Back to the initial state: Fit zoom, origin-aligned (the image (0,0)
    // coincides with the ruler origin), image still shown
    EXPECT_EQ(AppState::instance()->zoomMode(), AppState::Fit);
    const QPointF origin = viewer->mapImageToView(QPointF(0, 0));
    EXPECT_NEAR(origin.x(), 44.0, 0.5);
    EXPECT_NEAR(origin.y(), 20.0, 0.5);
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
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    // Zoom in first so panning is legal (at Fit the image fills the width
    // and the new pan clamp pins horizontal movement to 0)
    AppState::instance()->setZoomFactor(2.0);
    QCoreApplication::processEvents();

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

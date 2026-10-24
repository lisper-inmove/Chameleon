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
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QWheelEvent>
#include <QtTest/QTest>

#include <opencv2/imgproc.hpp>

#include "app/app_state.h"
#include "processing/image_io.h"
#include "ui/histogram_compare_dialog.h"
#include "ui/back_project_dialog.h"
#include "ui/convolution_dialog.h"
#include "ui/connected_components_dialog.h"
#include "ui/contour_dialog.h"
#include "ui/hough_dialog.h"
#include "ui/edge_dialog.h"
#include "ui/noise_dialog.h"
#include "ui/sharpen_dialog.h"
#include "ui/threshold_dialog.h"
#include "ui/geometry_dialog.h"
#include "ui/gradient_dialog.h"
#include "ui/histogram_match_dialog.h"
#include "ui/image_window.h"
#include "ui/histogram_window.h"
#include "ui/image_viewer_widget.h"
#include "ui/main_window.h"

using chameleon::AppState;
using chameleon::HistogramCompareDialog;
using chameleon::BackProjectDialog;
using chameleon::ConnectedComponentsDialog;
using chameleon::ContourDialog;
using chameleon::ConvolutionDialog;
using chameleon::HoughDialog;
using chameleon::EdgeDialog;
using chameleon::NoiseDialog;
using chameleon::SharpenDialog;
using chameleon::ThresholdDialog;
using chameleon::GeometryDialog;
using chameleon::GradientDialog;
using chameleon::HistogramMatchDialog;
using chameleon::ImageWindow;
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

// On-screen scale of one image pixel (independent of the zoom mode)
double displayedScale(ImageViewerWidget *viewer)
{
    const QPointF a = viewer->mapImageToView(QPointF(0, 0));
    const QPointF b = viewer->mapImageToView(QPointF(100, 0));
    return (b.x() - a.x()) / 100.0;
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
    // One merged side panel: thumbnails and the filter controls live in
    // tabs of a single dock
    EXPECT_NE(s_window->findChild<QDockWidget *>("panelDock"), nullptr);
    auto *tabs = s_window->findChild<QTabWidget *>("panelTabs");
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(tabs->count(), 2);
    EXPECT_EQ(tabs->tabText(0).toStdString(), "缩略图");
    EXPECT_EQ(tabs->tabText(1).toStdString(), "控制面板");
    EXPECT_EQ(s_window->findChildren<QDockWidget *>().size(), 1);
    // The display area is split: original on the left, processed on the right
    auto *splitter = s_window->findChild<QSplitter *>("viewSplitter");
    ASSERT_NE(splitter, nullptr);
    EXPECT_EQ(splitter->count(), 2);
    EXPECT_NE(s_window->findChild<ImageViewerWidget *>("originalViewer"), nullptr);
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
    auto *leftDock = s_window->findChild<QDockWidget *>("panelDock");
    auto *toggle = action("panelDockToggle");
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
    // Histogram features live in the toolbar, not in the context menu
    EXPECT_FALSE(texts.contains("灰度直方图"));
    EXPECT_FALSE(texts.contains("直方图均衡化"));
    EXPECT_FALSE(texts.contains("CLAHE 均衡化"));

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

    // Trigger 灰度直方图 from the toolbar's histogram menu
    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    EXPECT_NE(s_window->findChild<QToolButton *>("histogramButton"), nullptr);
    {
        bool triggered = false;
        for (auto *action : histogramMenu->actions()) {
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

    for (auto *action : histogramMenu->actions()) {
        if (action->text() == "灰度直方图")
            action->trigger();
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

TEST_F(MainWindowTest, EqualizationMenuActionsWork)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    bool foundHistogram = false;
    bool foundClahe = false;
    for (auto *action : histogramMenu->actions()) {
        if (action->text() == "直方图均衡化")
            foundHistogram = true;
        else if (action->text() == "CLAHE 均衡化")
            foundClahe = true;
    }
    EXPECT_TRUE(foundHistogram);
    EXPECT_TRUE(foundClahe);

    const QImage before = viewer->grab().toImage();
    for (auto *action : histogramMenu->actions()) {
        if (action->text() == "直方图均衡化")
            action->trigger();
    }
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());
}

TEST_F(MainWindowTest, HistogramCompareDialogComputesScores)
{
    // Two inputs: the reference (current image / selection) and a target
    // chosen from the opened images or browsed from disk
    const QString identical = QDir::tempPath() + "/chameleon_compare_same.png";
    const QString different = QDir::tempPath() + "/chameleon_compare_diff.png";
    QFile::remove(identical);
    QFile::remove(different);
    const cv::Mat color(16, 16, CV_8UC3, cv::Scalar(10, 20, 30));
    const cv::Mat other(16, 16, CV_8UC3, cv::Scalar(200, 210, 220));
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(identical.toStdString(), color));
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(different.toStdString(), other));

    HistogramCompareDialog dialog(color, QStringLiteral("当前显示图"),
                                  {identical, different});
    auto *methodCombo = dialog.findChild<QComboBox *>("compareMethodCombo");
    auto *targetCombo = dialog.findChild<QComboBox *>("compareTargetCombo");
    auto *results = dialog.findChild<QLabel *>("compareResultsLabel");
    ASSERT_NE(methodCombo, nullptr);
    ASSERT_NE(targetCombo, nullptr);
    ASSERT_NE(results, nullptr);
    EXPECT_GE(methodCombo->count(), 5);
    EXPECT_EQ(targetCombo->count(), 2);
    EXPECT_NE(dialog.findChild<QPushButton *>("compareBrowseButton"), nullptr);

    // Identical images -> normalized similarity 1.0 for every channel
    EXPECT_TRUE(results->text().contains(QStringLiteral("相似度")));
    EXPECT_TRUE(results->text().contains(QStringLiteral("1.0000")));
    EXPECT_TRUE(results->text().contains(QStringLiteral("平均")));

    // A disjoint target drops the similarity below 1
    targetCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    EXPECT_FALSE(results->text().contains(QStringLiteral("1.0000")));

    // Switching the method recomputes on the same target
    const QString beforeMethodSwitch = results->text();
    methodCombo->setCurrentIndex(1);  // 卡方
    QCoreApplication::processEvents();
    EXPECT_NE(results->text(), beforeMethodSwitch);
    EXPECT_TRUE(results->text().contains(QStringLiteral("相似度")));

    // Browsing adds a new target input and selects it
    const QString browsed = QDir::tempPath() + "/chameleon_compare_browse.png";
    QFile::remove(browsed);
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(browsed.toStdString(), color));
    dialog.addTargetPath(browsed);
    QCoreApplication::processEvents();
    EXPECT_EQ(targetCombo->count(), 3);
    EXPECT_EQ(targetCombo->currentIndex(), 2);
    EXPECT_TRUE(results->text().contains(QStringLiteral("1.0000")));

    QFile::remove(identical);
    QFile::remove(different);
    QFile::remove(browsed);
}

TEST_F(MainWindowTest, HistogramCompareMenuActionOpensDialog)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    bool found = false;
    for (auto *action : histogramMenu->actions()) {
        if (action->text() == "直方图比较...") {
            action->trigger();
            found = true;
        }
    }
    EXPECT_TRUE(found);
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<HistogramCompareDialog *>();
    ASSERT_NE(dialog, nullptr);
    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, HistogramMatchDialogAppliesToViewer)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    bool found = false;
    for (auto *action : histogramMenu->actions()) {
        if (action->text() == "直方图匹配...") {
            action->trigger();
            found = true;
        }
    }
    EXPECT_TRUE(found);
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<HistogramMatchDialog *>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_NE(dialog->findChild<QComboBox *>("matchTargetCombo"), nullptr);
    EXPECT_NE(dialog->findChild<QPushButton *>("matchBrowseButton"), nullptr);
    auto *applyButton = dialog->findChild<QPushButton *>("matchApplyButton");
    ASSERT_NE(applyButton, nullptr);

    // Choose a template from disk and apply it
    const QString templ = QDir::tempPath() + "/chameleon_match_template.png";
    QFile::remove(templ);
    const cv::Mat templateImage(16, 16, CV_8UC3, cv::Scalar(200, 200, 200));
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(templ.toStdString(), templateImage));
    dialog->addTargetPath(templ);

    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
    QFile::remove(templ);
}

TEST_F(MainWindowTest, BackProjectMenuActionShowsResultWindow)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    bool found = false;
    for (auto *action : histogramMenu->actions()) {
        if (action->text() == "直方图反向投影...") {
            action->trigger();
            found = true;
        }
    }
    EXPECT_TRUE(found);
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<BackProjectDialog *>();
    ASSERT_NE(dialog, nullptr);
    EXPECT_NE(dialog->findChild<QComboBox *>("backProjectTargetCombo"), nullptr);
    EXPECT_NE(dialog->findChild<QPushButton *>("backProjectBrowseButton"), nullptr);
    auto *computeButton = dialog->findChild<QPushButton *>("backProjectButton");
    ASSERT_NE(computeButton, nullptr);

    // Choose a white template and compute the back projection
    const QString templ = QDir::tempPath() + "/chameleon_backproject_template.png";
    QFile::remove(templ);
    const cv::Mat templateImage(16, 16, CV_8UC3, cv::Scalar(255, 255, 255));
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(templ.toStdString(), templateImage));
    dialog->addTargetPath(templ);

    computeButton->click();
    QCoreApplication::processEvents();
    auto *resultWindow = s_window->findChild<ImageWindow *>("backProjectionWindow");
    ASSERT_NE(resultWindow, nullptr);
    EXPECT_EQ(resultWindow->imageSize(), QSize(640, 400));

    // Recomputing reuses the same result window
    computeButton->click();
    QCoreApplication::processEvents();
    EXPECT_EQ(s_window->findChildren<ImageWindow *>("backProjectionWindow").size(), 1);

    resultWindow->close();
    dialog->close();
    QCoreApplication::processEvents();
    QFile::remove(templ);
}

TEST_F(MainWindowTest, ConvolutionDialogExposesParamsAndApplies)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *convolutionButton = s_window->findChild<QToolButton *>("convolutionButton");
    ASSERT_NE(convolutionButton, nullptr);
    convolutionButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<ConvolutionDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *typeCombo = dialog->findChild<QComboBox *>("convolutionTypeCombo");
    auto *kernelSpin = dialog->findChild<QSpinBox *>("convolutionKernelSpin");
    auto *sigmaXSpin = dialog->findChild<QDoubleSpinBox *>("convolutionSigmaXSpin");
    auto *sigmaYSpin = dialog->findChild<QDoubleSpinBox *>("convolutionSigmaYSpin");
    auto *deltaSpin = dialog->findChild<QDoubleSpinBox *>("convolutionDeltaSpin");
    auto *sigmaColorSpin = dialog->findChild<QDoubleSpinBox *>("convolutionSigmaColorSpin");
    auto *sigmaSpaceSpin = dialog->findChild<QDoubleSpinBox *>("convolutionSigmaSpaceSpin");
    auto *borderCombo = dialog->findChild<QComboBox *>("convolutionBorderTypeCombo");
    auto *cell = dialog->findChild<QDoubleSpinBox *>("convolutionKernelCell0");
    auto *applyButton = dialog->findChild<QPushButton *>("convolutionApplyButton");
    ASSERT_NE(typeCombo, nullptr);
    ASSERT_NE(kernelSpin, nullptr);
    ASSERT_NE(sigmaXSpin, nullptr);
    ASSERT_NE(sigmaYSpin, nullptr);
    ASSERT_NE(deltaSpin, nullptr);
    ASSERT_NE(sigmaColorSpin, nullptr);
    ASSERT_NE(sigmaSpaceSpin, nullptr);
    ASSERT_NE(borderCombo, nullptr);
    ASSERT_NE(cell, nullptr);
    ASSERT_NE(applyButton, nullptr);
    EXPECT_EQ(typeCombo->count(), 8);
    EXPECT_GE(borderCombo->count(), 3);

    // Gaussian (default): sigma editable, custom kernel disabled
    EXPECT_TRUE(sigmaXSpin->isEnabled());
    EXPECT_FALSE(cell->isEnabled());
    EXPECT_FALSE(deltaSpin->isEnabled());

    // Custom: kernel cells and delta enabled, sigma and kernel size disabled
    typeCombo->setCurrentIndex(3);
    QCoreApplication::processEvents();
    EXPECT_TRUE(cell->isEnabled());
    EXPECT_TRUE(deltaSpin->isEnabled());
    EXPECT_FALSE(sigmaXSpin->isEnabled());
    EXPECT_FALSE(kernelSpin->isEnabled());

    // Directional blur: kernel size editable, everything else disabled
    typeCombo->setCurrentIndex(4);
    QCoreApplication::processEvents();
    EXPECT_TRUE(kernelSpin->isEnabled());
    EXPECT_FALSE(sigmaXSpin->isEnabled());
    EXPECT_FALSE(cell->isEnabled());
    EXPECT_FALSE(deltaSpin->isEnabled());

    // Median: apply and check the effect on the viewer
    typeCombo->setCurrentIndex(2);
    QCoreApplication::processEvents();
    EXPECT_TRUE(kernelSpin->isEnabled());
    kernelSpin->setValue(3);

    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, GradientDialogExposesParamsAndApplies)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *gradientButton = s_window->findChild<QToolButton *>("gradientButton");
    ASSERT_NE(gradientButton, nullptr);
    gradientButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<GradientDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *typeCombo = dialog->findChild<QComboBox *>("gradientTypeCombo");
    auto *dxSpin = dialog->findChild<QSpinBox *>("gradientDxSpin");
    auto *dySpin = dialog->findChild<QSpinBox *>("gradientDySpin");
    auto *ksizeSpin = dialog->findChild<QSpinBox *>("gradientKsizeSpin");
    auto *scaleSpin = dialog->findChild<QDoubleSpinBox *>("gradientScaleSpin");
    auto *deltaSpin = dialog->findChild<QDoubleSpinBox *>("gradientDeltaSpin");
    auto *applyButton = dialog->findChild<QPushButton *>("gradientApplyButton");
    ASSERT_NE(typeCombo, nullptr);
    ASSERT_NE(dxSpin, nullptr);
    ASSERT_NE(dySpin, nullptr);
    ASSERT_NE(ksizeSpin, nullptr);
    ASSERT_NE(scaleSpin, nullptr);
    ASSERT_NE(deltaSpin, nullptr);
    ASSERT_NE(applyButton, nullptr);
    EXPECT_EQ(typeCombo->count(), 3);

    // Sobel (default): the kernel size is editable
    EXPECT_TRUE(ksizeSpin->isEnabled());

    // Scharr: fixed 3x3 kernel, the rest stays editable
    typeCombo->setCurrentIndex(2);
    QCoreApplication::processEvents();
    EXPECT_FALSE(ksizeSpin->isEnabled());
    EXPECT_TRUE(dxSpin->isEnabled());
    EXPECT_TRUE(dySpin->isEnabled());
    EXPECT_TRUE(scaleSpin->isEnabled());
    EXPECT_TRUE(deltaSpin->isEnabled());

    // Back to Sobel and apply: the viewer image changes and is undoable
    typeCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, EdgeDialogExposesParamsAndApplies)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *edgeButton = s_window->findChild<QToolButton *>("edgeButton");
    ASSERT_NE(edgeButton, nullptr);
    edgeButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<EdgeDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *threshold1 = dialog->findChild<QSpinBox *>("edgeThreshold1Spin");
    auto *threshold2 = dialog->findChild<QSpinBox *>("edgeThreshold2Spin");
    auto *aperture = dialog->findChild<QSpinBox *>("edgeApertureSpin");
    auto *l2Gradient = dialog->findChild<QCheckBox *>("edgeL2GradientCheck");
    auto *applyButton = dialog->findChild<QPushButton *>("edgeApplyButton");
    ASSERT_NE(threshold1, nullptr);
    ASSERT_NE(threshold2, nullptr);
    ASSERT_NE(aperture, nullptr);
    ASSERT_NE(l2Gradient, nullptr);
    ASSERT_NE(applyButton, nullptr);
    EXPECT_EQ(aperture->minimum(), 3);
    EXPECT_EQ(aperture->maximum(), 7);
    EXPECT_FALSE(l2Gradient->isChecked());  // default L1

    // Every parameter is settable from the dialog
    threshold1->setValue(40);
    threshold2->setValue(120);
    aperture->setValue(5);
    l2Gradient->setChecked(true);

    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, NoiseDialogExposesParamsAndApplies)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *noiseButton = s_window->findChild<QToolButton *>("noiseButton");
    ASSERT_NE(noiseButton, nullptr);
    noiseButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<NoiseDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *noiseTypeCombo = dialog->findChild<QComboBox *>("noiseTypeCombo");
    auto *proportionSpin = dialog->findChild<QDoubleSpinBox *>("noiseProportionSpin");
    auto *meanSpin = dialog->findChild<QDoubleSpinBox *>("noiseMeanSpin");
    auto *sigmaSpin = dialog->findChild<QDoubleSpinBox *>("noiseSigmaSpin");
    auto *denoiseTypeCombo = dialog->findChild<QComboBox *>("denoiseTypeCombo");
    auto *denoiseKernelSpin = dialog->findChild<QSpinBox *>("denoiseKernelSpin");
    auto *addNoiseButton = dialog->findChild<QPushButton *>("addNoiseButton");
    auto *denoiseButton = dialog->findChild<QPushButton *>("denoiseButton");
    ASSERT_NE(noiseTypeCombo, nullptr);
    ASSERT_NE(proportionSpin, nullptr);
    ASSERT_NE(meanSpin, nullptr);
    ASSERT_NE(sigmaSpin, nullptr);
    ASSERT_NE(denoiseTypeCombo, nullptr);
    ASSERT_NE(denoiseKernelSpin, nullptr);
    ASSERT_NE(addNoiseButton, nullptr);
    ASSERT_NE(denoiseButton, nullptr);
    EXPECT_EQ(noiseTypeCombo->count(), 2);
    EXPECT_EQ(denoiseTypeCombo->count(), 2);

    // Salt-pepper (default): proportion editable, Gaussian params disabled
    EXPECT_TRUE(proportionSpin->isEnabled());
    EXPECT_FALSE(meanSpin->isEnabled());
    EXPECT_FALSE(sigmaSpin->isEnabled());

    // Gaussian: swapped visibility
    noiseTypeCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    EXPECT_FALSE(proportionSpin->isEnabled());
    EXPECT_TRUE(meanSpin->isEnabled());
    EXPECT_TRUE(sigmaSpin->isEnabled());

    // Add Gaussian noise, then denoise with the median filter
    const QImage clean = viewer->grab().toImage();
    addNoiseButton->click();
    QCoreApplication::processEvents();
    const QImage noisy = viewer->grab().toImage();
    EXPECT_NE(noisy, clean);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    denoiseButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), noisy);

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, ToolbarDialogsAreUnique)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto clickTwice = [this](const char *buttonName) {
        auto *button = s_window->findChild<QToolButton *>(buttonName);
        EXPECT_NE(button, nullptr);
        if (button) {
            button->click();
            QCoreApplication::processEvents();
            button->click();
            QCoreApplication::processEvents();
        }
    };

    clickTwice("convolutionButton");
    clickTwice("gradientButton");
    clickTwice("edgeButton");
    clickTwice("noiseButton");
    EXPECT_EQ(s_window->findChildren<ConvolutionDialog *>().size(), 1);
    EXPECT_EQ(s_window->findChildren<GradientDialog *>().size(), 1);
    EXPECT_EQ(s_window->findChildren<EdgeDialog *>().size(), 1);
    EXPECT_EQ(s_window->findChildren<NoiseDialog *>().size(), 1);

    // Reopening reuses the dialog: in-progress parameter edits survive
    auto *dialog = s_window->findChild<ConvolutionDialog *>();
    auto *kernelSpin = dialog->findChild<QSpinBox *>("convolutionKernelSpin");
    ASSERT_NE(kernelSpin, nullptr);
    kernelSpin->setValue(7);
    clickTwice("convolutionButton");
    EXPECT_EQ(s_window->findChildren<ConvolutionDialog *>().size(), 1);
    EXPECT_EQ(s_window->findChild<QSpinBox *>("convolutionKernelSpin")->value(), 7);

    for (auto *d : s_window->findChildren<QDialog *>())
        d->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, HistogramMenuDialogsAreUnique)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    auto triggerTwice = [histogramMenu](const QString &text) {
        for (int round = 0; round < 2; ++round) {
            for (auto *action : histogramMenu->actions()) {
                if (action->text() == text)
                    action->trigger();
            }
            QCoreApplication::processEvents();
        }
    };

    triggerTwice(QStringLiteral("直方图比较..."));
    triggerTwice(QStringLiteral("直方图匹配..."));
    triggerTwice(QStringLiteral("直方图反向投影..."));
    EXPECT_EQ(s_window->findChildren<HistogramCompareDialog *>().size(), 1);
    EXPECT_EQ(s_window->findChildren<HistogramMatchDialog *>().size(), 1);
    EXPECT_EQ(s_window->findChildren<BackProjectDialog *>().size(), 1);

    for (auto *d : s_window->findChildren<QDialog *>())
        d->close();
    for (auto *w : s_window->findChildren<ImageWindow *>())
        w->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, HistogramWindowsAreReusedAndUpdated)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    auto showHistograms = [histogramMenu] {
        for (auto *action : histogramMenu->actions()) {
            if (action->text() == "灰度直方图")
                action->trigger();
        }
        QCoreApplication::processEvents();
    };

    showHistograms();
    EXPECT_EQ(s_window->findChildren<HistogramWindow *>().size(), 3);

    // Select the 20x20 background corner, then show again: the same three
    // windows must be reused and updated to the new data
    const QPoint start = viewer->mapImageToView(QPointF(0, 0)).toPoint();
    const QPoint end = viewer->mapImageToView(QPointF(20, 20)).toPoint();
    QTest::mousePress(viewer, Qt::RightButton, Qt::NoModifier, start);
    QTest::mouseMove(viewer, end);
    QTest::mouseRelease(viewer, Qt::RightButton, Qt::NoModifier, end);
    ASSERT_TRUE(viewer->hasSelection());

    showHistograms();
    EXPECT_EQ(s_window->findChildren<HistogramWindow *>().size(), 3);
    auto *blue = s_window->findChild<HistogramWindow *>("histogramWindow_blue");
    ASSERT_NE(blue, nullptr);
    EXPECT_DOUBLE_EQ(blue->bins()[200], 400);  // updated to the selection

    for (auto *w : s_window->findChildren<HistogramWindow *>())
        w->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, SharpenDialogExposesParamsAndSharpness)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *sharpenButton = s_window->findChild<QToolButton *>("sharpenButton");
    ASSERT_NE(sharpenButton, nullptr);
    sharpenButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<SharpenDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *ksizeSpin = dialog->findChild<QSpinBox *>("sharpenKsizeSpin");
    auto *strengthSpin = dialog->findChild<QDoubleSpinBox *>("sharpenStrengthSpin");
    auto *applyButton = dialog->findChild<QPushButton *>("sharpenApplyButton");
    auto *measureButton = dialog->findChild<QPushButton *>("sharpenMeasureButton");
    auto *sharpnessLabel = dialog->findChild<QLabel *>("sharpenSharpnessLabel");
    ASSERT_NE(ksizeSpin, nullptr);
    ASSERT_NE(strengthSpin, nullptr);
    ASSERT_NE(applyButton, nullptr);
    ASSERT_NE(measureButton, nullptr);
    ASSERT_NE(sharpnessLabel, nullptr);
    EXPECT_EQ(ksizeSpin->minimum(), 1);
    EXPECT_EQ(ksizeSpin->maximum(), 7);

    // The sharpness of the current image is shown right away and can be
    // recomputed on demand
    EXPECT_TRUE(sharpnessLabel->text().contains(QStringLiteral("锐度")));
    measureButton->click();
    QCoreApplication::processEvents();
    EXPECT_TRUE(sharpnessLabel->text().contains(QStringLiteral("锐度")));

    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, SplitViewShowsOriginalAndProcessed)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *original = s_window->findChild<ImageViewerWidget *>("originalViewer");
    auto *processed = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_NE(original, nullptr);
    ASSERT_NE(processed, nullptr);
    ASSERT_TRUE(waitForStatus(original, ImageViewerWidget::Ready));
    ASSERT_TRUE(waitForStatus(processed, ImageViewerWidget::Ready));

    // Freshly loaded both sides show the original image
    EXPECT_EQ(original->imagePath(), processed->imagePath());

    // After processing only the right side changes; the left keeps the
    // original
    const QImage originalBefore = original->grab().toImage();
    const QImage processedBefore = processed->grab().toImage();
    processed->equalizeHistogram();
    QCoreApplication::processEvents();
    EXPECT_NE(processed->grab().toImage(), processedBefore);
    EXPECT_EQ(original->grab().toImage(), originalBefore);
}

TEST_F(MainWindowTest, ProcessingSurvivesTabSwitch)
{
    // The reported bug: process one of several open images, switch to
    // another tab, switch back - the processing must still be there
    const QString pathA = QDir::tempPath() + "/chameleon_tabs_a.png";
    const QString pathB = QDir::tempPath() + "/chameleon_tabs_b.png";
    QFile::remove(pathA);
    QFile::remove(pathB);
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", pathA));
    ASSERT_TRUE(QFile::copy(TEST_DATA_DIR "/test_image.png", pathB));

    AppState::instance()->openImages({pathA, pathB});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
    const QImage pristine = viewer->grab().toImage();

    viewer->equalizeHistogram();
    const QImage edited = viewer->grab().toImage();
    ASSERT_NE(edited, pristine);

    AppState::instance()->setCurrentIndex(1);  // switch to the other tab
    QCoreApplication::processEvents();
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(viewer->grab().toImage(), pristine);  // untouched image

    AppState::instance()->setCurrentIndex(0);  // and back
    QCoreApplication::processEvents();
    EXPECT_EQ(viewer->grab().toImage(), edited);  // edits restored

    while (!AppState::instance()->images().isEmpty())
        AppState::instance()->closeImage(0);
    QFile::remove(pathA);
    QFile::remove(pathB);
}

TEST_F(MainWindowTest, ReloadDiscardsProcessingEdits)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
    const QImage pristine = viewer->grab().toImage();

    viewer->equalizeHistogram();
    ASSERT_NE(viewer->grab().toImage(), pristine);

    action("reloadAction")->trigger();
    QCoreApplication::processEvents();
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));
    EXPECT_EQ(viewer->grab().toImage(), pristine);
}

TEST_F(MainWindowTest, ThresholdDialogExposesParamsAndApplies)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *thresholdButton = s_window->findChild<QToolButton *>("thresholdButton");
    ASSERT_NE(thresholdButton, nullptr);
    thresholdButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<ThresholdDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *threshSpin = dialog->findChild<QDoubleSpinBox *>("thresholdValueSpin");
    auto *maxvalSpin = dialog->findChild<QDoubleSpinBox *>("thresholdMaxvalSpin");
    auto *typeCombo = dialog->findChild<QComboBox *>("thresholdTypeCombo");
    auto *applyButton = dialog->findChild<QPushButton *>("thresholdApplyButton");
    ASSERT_NE(threshSpin, nullptr);
    ASSERT_NE(maxvalSpin, nullptr);
    ASSERT_NE(typeCombo, nullptr);
    ASSERT_NE(applyButton, nullptr);
    EXPECT_EQ(typeCombo->count(), 5);

    // All three parameters are settable
    threshSpin->setValue(120);
    maxvalSpin->setValue(200);
    typeCombo->setCurrentIndex(1);  // 反二值化

    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, ThresholdDialogExposesAutoMethods)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *thresholdButton = s_window->findChild<QToolButton *>("thresholdButton");
    ASSERT_NE(thresholdButton, nullptr);
    thresholdButton->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<ThresholdDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *methodCombo = dialog->findChild<QComboBox *>("thresholdMethodCombo");
    auto *threshSpin = dialog->findChild<QDoubleSpinBox *>("thresholdValueSpin");
    auto *blockSpin = dialog->findChild<QSpinBox *>("thresholdBlockSizeSpin");
    auto *cSpin = dialog->findChild<QDoubleSpinBox *>("thresholdCSpin");
    auto *computedLabel = dialog->findChild<QLabel *>("thresholdComputedLabel");
    auto *applyButton = dialog->findChild<QPushButton *>("thresholdApplyButton");
    ASSERT_NE(methodCombo, nullptr);
    ASSERT_NE(threshSpin, nullptr);
    ASSERT_NE(blockSpin, nullptr);
    ASSERT_NE(cSpin, nullptr);
    ASSERT_NE(computedLabel, nullptr);
    ASSERT_NE(applyButton, nullptr);
    EXPECT_EQ(methodCombo->count(), 4);

    // Fixed (default): the manual threshold is editable
    EXPECT_TRUE(threshSpin->isEnabled());
    EXPECT_FALSE(blockSpin->isEnabled());

    // Otsu: the manual threshold is disabled, apply reports the computed one
    methodCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    EXPECT_FALSE(threshSpin->isEnabled());
    EXPECT_FALSE(blockSpin->isEnabled());
    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(computedLabel->text().contains(QStringLiteral("计算阈值")));

    // Adaptive: block size and C are editable
    methodCombo->setCurrentIndex(3);
    QCoreApplication::processEvents();
    EXPECT_TRUE(blockSpin->isEnabled());
    EXPECT_TRUE(cSpin->isEnabled());
    EXPECT_FALSE(threshSpin->isEnabled());
    const QImage beforeAdaptive = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), beforeAdaptive);

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, ConnectedComponentsDialogComputesAndShows)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *button = s_window->findChild<QToolButton *>("connectedComponentsButton");
    ASSERT_NE(button, nullptr);
    button->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<ConnectedComponentsDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *connectivityCombo = dialog->findChild<QComboBox *>("ccConnectivityCombo");
    auto *methodCombo = dialog->findChild<QComboBox *>("ccMethodCombo");
    auto *computeButton = dialog->findChild<QPushButton *>("ccComputeButton");
    auto *statsLabel = dialog->findChild<QLabel *>("ccStatsLabel");
    ASSERT_NE(connectivityCombo, nullptr);
    ASSERT_NE(methodCombo, nullptr);
    ASSERT_NE(computeButton, nullptr);
    ASSERT_NE(statsLabel, nullptr);
    EXPECT_EQ(connectivityCombo->count(), 2);
    EXPECT_EQ(methodCombo->count(), 2);

    // Plain labeling: the colorized map opens in the shared result window
    computeButton->click();
    QCoreApplication::processEvents();
    auto *resultWindow = s_window->findChild<ImageWindow *>("connectedComponentsWindow");
    ASSERT_NE(resultWindow, nullptr);
    EXPECT_EQ(resultWindow->imageSize(), QSize(640, 400));
    EXPECT_TRUE(statsLabel->text().contains(QStringLiteral("组件数")));

    // With statistics: areas and boxes are reported
    methodCombo->setCurrentIndex(1);
    QCoreApplication::processEvents();
    computeButton->click();
    QCoreApplication::processEvents();
    EXPECT_TRUE(statsLabel->text().contains(QStringLiteral("面积")));
    EXPECT_EQ(s_window->findChildren<ImageWindow *>("connectedComponentsWindow").size(), 1);

    resultWindow->close();
    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, ContourDialogAppliesAndShowsTheCount)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *button = s_window->findChild<QToolButton *>("contourButton");
    ASSERT_NE(button, nullptr);
    button->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<ContourDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *modeCombo = dialog->findChild<QComboBox *>("contourModeCombo");
    auto *methodCombo = dialog->findChild<QComboBox *>("contourMethodCombo");
    auto *offsetXSpin = dialog->findChild<QSpinBox *>("contourOffsetXSpin");
    auto *offsetYSpin = dialog->findChild<QSpinBox *>("contourOffsetYSpin");
    auto *thresholdSpin = dialog->findChild<QSpinBox *>("contourThresholdSpin");
    auto *otsuCheck = dialog->findChild<QCheckBox *>("contourOtsuCheck");
    auto *thicknessSpin = dialog->findChild<QSpinBox *>("contourThicknessSpin");
    auto *lineTypeCombo = dialog->findChild<QComboBox *>("contourLineTypeCombo");
    auto *largestCheck = dialog->findChild<QCheckBox *>("contourLargestCheck");
    auto *applyButton = dialog->findChild<QPushButton *>("contourApplyButton");
    auto *countLabel = dialog->findChild<QLabel *>("contourCountLabel");
    auto *statsTable = dialog->findChild<QTableWidget *>("contourStatsTable");
    ASSERT_NE(modeCombo, nullptr);
    ASSERT_NE(methodCombo, nullptr);
    ASSERT_NE(offsetXSpin, nullptr);
    ASSERT_NE(offsetYSpin, nullptr);
    ASSERT_NE(thresholdSpin, nullptr);
    ASSERT_NE(otsuCheck, nullptr);
    ASSERT_NE(thicknessSpin, nullptr);
    ASSERT_NE(lineTypeCombo, nullptr);
    ASSERT_NE(largestCheck, nullptr);
    ASSERT_NE(applyButton, nullptr);
    ASSERT_NE(countLabel, nullptr);
    ASSERT_NE(statsTable, nullptr);

    // All cv::findContours parameters are exposed
    EXPECT_EQ(modeCombo->count(), 4);    // EXTERNAL / LIST / CCOMP / TREE
    EXPECT_EQ(methodCombo->count(), 4);  // NONE / SIMPLE / TC89-L1 / TC89-KCOS
    EXPECT_EQ(lineTypeCombo->count(), 3);
    EXPECT_TRUE(otsuCheck->isChecked());
    EXPECT_FALSE(thresholdSpin->isEnabled());  // Otsu ignores the manual value
    otsuCheck->setChecked(false);
    QCoreApplication::processEvents();
    EXPECT_TRUE(thresholdSpin->isEnabled());

    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    EXPECT_TRUE(countLabel->text().contains(QStringLiteral("轮廓")));
    EXPECT_TRUE(action("undoAction")->isEnabled());

    // Every contour is listed with its number, area, perimeter, box and
    // the minAreaRect shape descriptors
    EXPECT_EQ(statsTable->columnCount(), 8);
    ASSERT_GT(statsTable->rowCount(), 0);
    EXPECT_EQ(statsTable->item(0, 0)->text(), QStringLiteral("1"));
    EXPECT_FALSE(statsTable->item(0, 1)->text().isEmpty());  // area
    EXPECT_FALSE(statsTable->item(0, 2)->text().isEmpty());  // perimeter
    EXPECT_FALSE(statsTable->item(0, 3)->text().isEmpty());  // bounding box
    EXPECT_FALSE(statsTable->item(0, 4)->text().isEmpty());  // aspect ratio
    EXPECT_FALSE(statsTable->item(0, 5)->text().isEmpty());  // extent
    EXPECT_FALSE(statsTable->item(0, 6)->text().isEmpty());  // solidity
    EXPECT_FALSE(statsTable->item(0, 7)->text().isEmpty());  // pixel mean

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, ContourFitButtonsApplyAndListResults)
{
    // A single filled circle: exactly one valid contour for every fit
    const QString path = QDir::tempPath() + "/chameleon_fit_ui.png";
    QFile::remove(path);
    cv::Mat source(240, 320, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::circle(source, cv::Point(160, 120), 60, cv::Scalar(255, 255, 255),
               cv::FILLED);
    ASSERT_TRUE(chameleon::ImageIO::imwriteUtf8(path.toStdString(), source));

    AppState::instance()->openImages({path});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *button = s_window->findChild<QToolButton *>("contourButton");
    ASSERT_NE(button, nullptr);
    button->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<ContourDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *otsuCheck = dialog->findChild<QCheckBox *>("contourOtsuCheck");
    auto *thresholdSpin = dialog->findChild<QSpinBox *>("contourThresholdSpin");
    auto *ellipseButton =
        dialog->findChild<QPushButton *>("contourFitEllipseButton");
    auto *lineButton =
        dialog->findChild<QPushButton *>("contourFitLineButton");
    auto *polyButton =
        dialog->findChild<QPushButton *>("contourFitPolyButton");
    auto *distCombo = dialog->findChild<QComboBox *>("contourLineDistCombo");
    auto *paramSpin =
        dialog->findChild<QDoubleSpinBox *>("contourLineParamSpin");
    auto *repsSpin =
        dialog->findChild<QDoubleSpinBox *>("contourLineRepsSpin");
    auto *aepsSpin =
        dialog->findChild<QDoubleSpinBox *>("contourLineAepsSpin");
    auto *epsilonSpin =
        dialog->findChild<QDoubleSpinBox *>("contourPolyEpsilonSpin");
    auto *closedCheck =
        dialog->findChild<QCheckBox *>("contourPolyClosedCheck");
    auto *fitThickness =
        dialog->findChild<QSpinBox *>("contourFitThicknessSpin");
    auto *fitTable = dialog->findChild<QTableWidget *>("contourFitTable");
    ASSERT_NE(otsuCheck, nullptr);
    ASSERT_NE(thresholdSpin, nullptr);
    ASSERT_NE(ellipseButton, nullptr);
    ASSERT_NE(lineButton, nullptr);
    ASSERT_NE(polyButton, nullptr);
    ASSERT_NE(distCombo, nullptr);
    ASSERT_NE(paramSpin, nullptr);
    ASSERT_NE(repsSpin, nullptr);
    ASSERT_NE(aepsSpin, nullptr);
    ASSERT_NE(epsilonSpin, nullptr);
    ASSERT_NE(closedCheck, nullptr);
    ASSERT_NE(fitThickness, nullptr);
    ASSERT_NE(fitTable, nullptr);

    // All fitLine distance types and approxPolyDP options are exposed
    EXPECT_EQ(distCombo->count(), 6);
    EXPECT_TRUE(closedCheck->isChecked());
    EXPECT_EQ(fitTable->columnCount(), 2);

    const QImage before = viewer->grab().toImage();
    ellipseButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    ASSERT_EQ(fitTable->rowCount(), 1);
    EXPECT_EQ(fitTable->item(0, 0)->text(), QStringLiteral("1"));
    EXPECT_TRUE(
        fitTable->item(0, 1)->text().contains(QStringLiteral("中心")));

    polyButton->click();
    QCoreApplication::processEvents();
    ASSERT_EQ(fitTable->rowCount(), 1);
    EXPECT_TRUE(
        fitTable->item(0, 1)->text().contains(QStringLiteral("顶点数")));

    lineButton->click();
    QCoreApplication::processEvents();
    ASSERT_EQ(fitTable->rowCount(), 1);
    EXPECT_TRUE(
        fitTable->item(0, 1)->text().contains(QStringLiteral("方向")));
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();
    QFile::remove(path);
}

TEST_F(MainWindowTest, HoughDialogExposesParametersAndApplies)
{
    // A vertical line for the HoughLines mode
    const QString linePath = QDir::tempPath() + "/chameleon_hough_line.png";
    QFile::remove(linePath);
    cv::Mat lineImage(200, 200, CV_8UC1, cv::Scalar(0));
    cv::line(lineImage, cv::Point(100, 20), cv::Point(100, 180),
             cv::Scalar(255), 3);
    ASSERT_TRUE(
        chameleon::ImageIO::imwriteUtf8(linePath.toStdString(), lineImage));

    AppState::instance()->openImages({linePath});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *button = s_window->findChild<QToolButton *>("houghButton");
    ASSERT_NE(button, nullptr);
    button->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<HoughDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *modeCombo = dialog->findChild<QComboBox *>("houghModeCombo");
    auto *rhoSpin = dialog->findChild<QDoubleSpinBox *>("houghRhoSpin");
    auto *thetaSpin = dialog->findChild<QDoubleSpinBox *>("houghThetaSpin");
    auto *thresholdSpin = dialog->findChild<QSpinBox *>("houghThresholdSpin");
    auto *srnSpin = dialog->findChild<QDoubleSpinBox *>("houghSrnSpin");
    auto *stnSpin = dialog->findChild<QDoubleSpinBox *>("houghStnSpin");
    auto *minThetaSpin =
        dialog->findChild<QDoubleSpinBox *>("houghMinThetaSpin");
    auto *maxThetaSpin =
        dialog->findChild<QDoubleSpinBox *>("houghMaxThetaSpin");
    auto *canny1Spin = dialog->findChild<QSpinBox *>("houghCanny1Spin");
    auto *canny2Spin = dialog->findChild<QSpinBox *>("houghCanny2Spin");
    auto *useEdgeValCheck =
        dialog->findChild<QCheckBox *>("houghUseEdgeValCheck");
    auto *lineThickness =
        dialog->findChild<QSpinBox *>("houghLineThicknessSpin");
    auto *methodCombo = dialog->findChild<QComboBox *>("houghMethodCombo");
    auto *dpSpin = dialog->findChild<QDoubleSpinBox *>("houghDpSpin");
    auto *minDistSpin = dialog->findChild<QDoubleSpinBox *>("houghMinDistSpin");
    auto *param1Spin = dialog->findChild<QDoubleSpinBox *>("houghParam1Spin");
    auto *param2Spin = dialog->findChild<QDoubleSpinBox *>("houghParam2Spin");
    auto *minRadiusSpin = dialog->findChild<QSpinBox *>("houghMinRadiusSpin");
    auto *maxRadiusSpin = dialog->findChild<QSpinBox *>("houghMaxRadiusSpin");
    auto *circleThickness =
        dialog->findChild<QSpinBox *>("houghCircleThicknessSpin");
    auto *applyButton = dialog->findChild<QPushButton *>("houghApplyButton");
    auto *resultTable = dialog->findChild<QTableWidget *>("houghResultTable");
    auto *countLabel = dialog->findChild<QLabel *>("houghCountLabel");
    ASSERT_NE(modeCombo, nullptr);
    ASSERT_NE(rhoSpin, nullptr);
    ASSERT_NE(thetaSpin, nullptr);
    ASSERT_NE(thresholdSpin, nullptr);
    ASSERT_NE(srnSpin, nullptr);
    ASSERT_NE(stnSpin, nullptr);
    ASSERT_NE(minThetaSpin, nullptr);
    ASSERT_NE(maxThetaSpin, nullptr);
    ASSERT_NE(canny1Spin, nullptr);
    ASSERT_NE(canny2Spin, nullptr);
    ASSERT_NE(useEdgeValCheck, nullptr);
    ASSERT_NE(lineThickness, nullptr);
    ASSERT_NE(methodCombo, nullptr);
    ASSERT_NE(dpSpin, nullptr);
    ASSERT_NE(minDistSpin, nullptr);
    ASSERT_NE(param1Spin, nullptr);
    ASSERT_NE(param2Spin, nullptr);
    ASSERT_NE(minRadiusSpin, nullptr);
    ASSERT_NE(maxRadiusSpin, nullptr);
    ASSERT_NE(circleThickness, nullptr);
    ASSERT_NE(applyButton, nullptr);
    ASSERT_NE(resultTable, nullptr);
    ASSERT_NE(countLabel, nullptr);
    EXPECT_EQ(modeCombo->count(), 2);
    EXPECT_EQ(methodCombo->count(), 2);

    // Default mode: Hough lines
    const QImage before = viewer->grab().toImage();
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_NE(viewer->grab().toImage(), before);
    ASSERT_GT(resultTable->rowCount(), 0);
    EXPECT_TRUE(countLabel->text().contains(QStringLiteral("直线")));
    EXPECT_TRUE(action("undoAction")->isEnabled());

    dialog->close();
    QCoreApplication::processEvents();

    // Circle mode on a ring image
    const QString circlePath =
        QDir::tempPath() + "/chameleon_hough_circle.png";
    QFile::remove(circlePath);
    cv::Mat circleImage(200, 200, CV_8UC1, cv::Scalar(0));
    cv::circle(circleImage, cv::Point(100, 100), 30, cv::Scalar(255), 3);
    ASSERT_TRUE(
        chameleon::ImageIO::imwriteUtf8(circlePath.toStdString(), circleImage));
    AppState::instance()->openImages({circlePath});
    QCoreApplication::processEvents();
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    button->click();
    QCoreApplication::processEvents();
    dialog = s_window->findChild<HoughDialog *>();
    ASSERT_NE(dialog, nullptr);
    modeCombo = dialog->findChild<QComboBox *>("houghModeCombo");
    param2Spin = dialog->findChild<QDoubleSpinBox *>("houghParam2Spin");
    auto *minRadius = dialog->findChild<QSpinBox *>("houghMinRadiusSpin");
    auto *maxRadius = dialog->findChild<QSpinBox *>("houghMaxRadiusSpin");
    applyButton = dialog->findChild<QPushButton *>("houghApplyButton");
    resultTable = dialog->findChild<QTableWidget *>("houghResultTable");
    countLabel = dialog->findChild<QLabel *>("houghCountLabel");
    ASSERT_NE(modeCombo, nullptr);
    ASSERT_NE(param2Spin, nullptr);
    ASSERT_NE(minRadius, nullptr);
    ASSERT_NE(maxRadius, nullptr);
    ASSERT_NE(applyButton, nullptr);
    ASSERT_NE(resultTable, nullptr);
    ASSERT_NE(countLabel, nullptr);

    modeCombo->setCurrentIndex(1);
    param2Spin->setValue(20.0);
    minRadius->setValue(20);
    maxRadius->setValue(40);
    QCoreApplication::processEvents();
    applyButton->click();
    QCoreApplication::processEvents();
    ASSERT_GT(resultTable->rowCount(), 0);
    EXPECT_TRUE(countLabel->text().contains(QStringLiteral("圆")));

    dialog->close();
    QCoreApplication::processEvents();
    QFile::remove(linePath);
    QFile::remove(circlePath);
}

TEST_F(MainWindowTest, GeometryDialogAppliesRotateAndUndoes)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *button = s_window->findChild<QToolButton *>("geometryButton");
    ASSERT_NE(button, nullptr);
    button->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<GeometryDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *modeCombo = dialog->findChild<QComboBox *>("geometryModeCombo");
    auto *angleSpin = dialog->findChild<QDoubleSpinBox *>("geometryAngleSpin");
    auto *applyButton = dialog->findChild<QPushButton *>("geometryApplyButton");
    ASSERT_NE(modeCombo, nullptr);
    ASSERT_NE(angleSpin, nullptr);
    ASSERT_NE(applyButton, nullptr);
    EXPECT_EQ(modeCombo->count(), 6);

    // Rotate 90 degrees: the dimensions swap
    modeCombo->setCurrentIndex(1);  // 旋转
    QCoreApplication::processEvents();
    angleSpin->setValue(90);
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(400, 640));

    action("undoAction")->trigger();
    QCoreApplication::processEvents();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));

    dialog->close();
    QCoreApplication::processEvents();
}

TEST_F(MainWindowTest, Quick270RotatesByMinusNinetyDegrees)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *viewer = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_TRUE(waitForStatus(viewer, ImageViewerWidget::Ready));

    auto *button = s_window->findChild<QToolButton *>("geometryButton");
    ASSERT_NE(button, nullptr);
    button->click();
    QCoreApplication::processEvents();

    auto *dialog = s_window->findChild<GeometryDialog *>();
    ASSERT_NE(dialog, nullptr);
    auto *angleSpin = dialog->findChild<QDoubleSpinBox *>("geometryAngleSpin");
    auto *quick270 = dialog->findChild<QPushButton *>("geometryQuick270Button");
    auto *applyButton = dialog->findChild<QPushButton *>("geometryApplyButton");
    ASSERT_NE(angleSpin, nullptr);
    ASSERT_NE(quick270, nullptr);
    ASSERT_NE(applyButton, nullptr);

    // 90 then 270 (= -90) must come back to the original orientation
    angleSpin->setValue(90);
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(400, 640));

    quick270->click();
    EXPECT_DOUBLE_EQ(angleSpin->value(), 270.0);  // not clamped to 180
    applyButton->click();
    QCoreApplication::processEvents();
    EXPECT_EQ(AppState::instance()->imageInfo(), QSize(640, 400));

    dialog->close();
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

    auto *histogramMenu = s_window->findChild<QMenu *>("histogramMenu");
    ASSERT_NE(histogramMenu, nullptr);
    for (auto *action : histogramMenu->actions()) {
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

TEST_F(MainWindowTest, ZoomingOnePaneDoesNotAffectTheOther)
{
    AppState::instance()->openImages({TEST_DATA_DIR "/test_image.png"});
    QCoreApplication::processEvents();
    auto *original = s_window->findChild<ImageViewerWidget *>("originalViewer");
    auto *processed = s_window->findChild<ImageViewerWidget *>("imageViewer");
    ASSERT_NE(original, nullptr);
    ASSERT_NE(processed, nullptr);
    ASSERT_TRUE(waitForStatus(original, ImageViewerWidget::Ready));
    ASSERT_TRUE(waitForStatus(processed, ImageViewerWidget::Ready));

    const double originalBefore = displayedScale(original);

    // Zoom the processed pane through the shared state (toolbar path):
    // the original pane must keep its own scale
    AppState::instance()->setZoomFactor(2.0);
    QCoreApplication::processEvents();
    EXPECT_NEAR(displayedScale(processed), 2.0, 1e-6);
    EXPECT_NEAR(displayedScale(original), originalBefore, 1e-6);

    // Wheel-zoom over the original pane: only the original pane changes
    const QPoint center = original->rect().center();
    QWheelEvent wheelIn(center, original->mapToGlobal(center), QPoint(),
                        QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                        Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(original, &wheelIn);
    QCoreApplication::processEvents();
    EXPECT_GT(displayedScale(original), originalBefore);
    EXPECT_NEAR(displayedScale(processed), 2.0, 1e-6);
    EXPECT_DOUBLE_EQ(AppState::instance()->zoomFactor(), 2.0) << "shared zoom untouched";
}

TEST_F(MainWindowTest, ToolbarExposesOpenGLPreview)
{
    // The action is not clicked here: it opens a native GLFW window
    auto *button = s_window->findChild<QToolButton *>("openGLButton");
    ASSERT_NE(button, nullptr);
    EXPECT_EQ(button->text(), QStringLiteral("OpenGL..."));
}

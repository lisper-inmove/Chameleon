#include "ui/main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QSplitter>
#include <QStyleFactory>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <utility>

#include <opencv2/imgproc.hpp>

#include "app/app_state.h"
#include "processing/histogram.h"
#include "processing/image_io.h"
#include "ui/filter_panel.h"
#include "ui/back_project_dialog.h"
#include "ui/convolution_dialog.h"
#include "ui/edge_dialog.h"
#include "ui/gradient_dialog.h"
#include "ui/noise_dialog.h"
#include "ui/sharpen_dialog.h"
#include "ui/histogram_compare_dialog.h"
#include "ui/histogram_match_dialog.h"
#include "ui/histogram_window.h"
#include "ui/image_conversion.h"
#include "ui/image_window.h"
#include "ui/image_viewer_widget.h"
#include "utils/file_utils.h"

namespace chameleon {

namespace {

QPalette darkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Base, QColor(35, 35, 35));
    p.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    p.setColor(QPalette::ToolTipBase, QColor(25, 25, 25));
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    return p;
}

QString formatSize(qint64 bytes)
{
    if (bytes < 0)
        return {};
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < qint64(1024) * 1024)
        return QStringLiteral("%1 KB").arg(double(bytes) / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(double(bytes) / 1024.0 / 1024.0, 0, 'f', 1);
}

// Toolbar popups are unique: an already-open window of the given type
// (optionally by object name) is raised and reused instead of creating
// another instance
template <typename WindowT>
WindowT *focusExisting(QWidget *parent, const QString &name = QString())
{
    auto *existing = parent->findChild<WindowT *>(name);
    if (existing) {
        existing->raise();
        existing->activateWindow();
    }
    return existing;
}

// Per-item widget: thumbnail + name + close button.
// (No Q_OBJECT: moc does not support classes in anonymous namespaces.)
class ThumbnailItemWidget : public QWidget
{
public:
    using ClickHandler = std::function<void(int)>;
    using CloseHandler = std::function<void(int)>;

    ThumbnailItemWidget(int index, const QString &path,
                        ClickHandler onClick, CloseHandler onClose,
                        QWidget *parent = nullptr)
        : QWidget(parent)
        , m_index(index)
        , m_onClick(std::move(onClick))
        , m_onClose(std::move(onClose))
    {
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto *thumb = new QLabel(this);
        // Load and downscale with OpenCV (Qt image plugins are not used)
        const cv::Mat mat = ImageIO::imreadUtf8(path.toStdString());
        if (!mat.empty()) {
            cv::Mat thumbMat;
            const double scale = std::min(96.0 / mat.cols, 72.0 / mat.rows);
            if (scale < 1.0) {
                cv::resize(mat, thumbMat,
                           cv::Size(std::max(1, int(mat.cols * scale)),
                                    std::max(1, int(mat.rows * scale))),
                           0, 0, cv::INTER_AREA);
            } else {
                thumbMat = mat;
            }
            thumb->setPixmap(pixmapFromMat(thumbMat));
        }
        thumb->setAlignment(Qt::AlignCenter);

        auto *name = new QLabel(QFileInfo(path).fileName(), this);
        name->setWordWrap(true);

        auto *closeButton = new QToolButton(this);
        closeButton->setText(QStringLiteral("×"));
        closeButton->setAutoRaise(true);
        connect(closeButton, &QToolButton::clicked, this, [this] { m_onClose(m_index); });

        layout->addWidget(thumb);
        layout->addWidget(name, 1);
        layout->addWidget(closeButton);
        setFixedHeight(84);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QWidget::mousePressEvent(event);
        m_onClick(m_index);
    }

private:
    int m_index;
    ClickHandler m_onClick;
    CloseHandler m_onClose;
};

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // Docks first: the view menu uses their toggleViewAction() so the
    // check state always matches visibility (incl. title-bar closes)
    setupDocks();
    setupMenus();
    setupToolBar();
    setupStatusBar();

    // Display split: the loaded original on the left, the processed
    // result on the right; freshly loaded both sides show the original
    m_originalViewer = new ImageViewerWidget(this);
    m_originalViewer->setObjectName("originalViewer");
    m_viewer = new ImageViewerWidget(this);
    m_viewer->setObjectName("imageViewer");
    // Keep each image's edits across tab switches
    m_viewer->setRememberStatePerPath(true);

    auto makePane = [this](const QString &title, const char *labelName,
                           ImageViewerWidget *viewer) {
        auto *pane = new QWidget(this);
        auto *layout = new QVBoxLayout(pane);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        auto *label = new QLabel(title, pane);
        label->setObjectName(labelName);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
        layout->addWidget(viewer, 1);
        return pane;
    };
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("viewSplitter");
    splitter->addWidget(makePane(QStringLiteral("原图"), "originalPaneLabel",
                                 m_originalViewer));
    splitter->addWidget(makePane(QStringLiteral("处理结果"), "processedPaneLabel",
                                 m_viewer));
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    auto *state = AppState::instance();
    connect(state, &AppState::currentIndexChanged, this, &MainWindow::onCurrentImageChanged);
    connect(state, &AppState::imagesChanged, this, &MainWindow::rebuildThumbnails);
    connect(state, &AppState::zoomChanged, this, &MainWindow::updateZoomUi);
    connect(state, &AppState::imageInfoChanged, this, &MainWindow::updateStatusBar);
    connect(state, &AppState::recentFilesChanged, this, &MainWindow::rebuildRecentFilesMenu);
    connect(state, &AppState::themeChanged, this, [this] { applyTheme(); });

    // Filter pipeline wiring: panel -> viewer params, viewer -> panel
    // reset mirroring and undo/redo action enablement
    connect(m_filterPanel, &FilterPanel::filterParamsChanged,
            m_viewer, &ImageViewerWidget::setFilterParams);
    connect(m_filterPanel, &FilterPanel::applyClicked,
            m_viewer, &ImageViewerWidget::applyFilter);
    connect(m_filterPanel, &FilterPanel::resetClicked, m_viewer, [this] {
        m_viewer->setFilterParams(0, 0, 0);
    });
    connect(m_viewer, &ImageViewerWidget::filterParamsReset,
            m_filterPanel, &FilterPanel::resetSliders);
    connect(m_viewer, &ImageViewerWidget::canUndoChanged,
            m_undoAction, &QAction::setEnabled);
    connect(m_viewer, &ImageViewerWidget::canRedoChanged,
            m_redoAction, &QAction::setEnabled);
    connect(m_viewer, &ImageViewerWidget::contextMenuRequested,
            this, &MainWindow::showViewerContextMenu);

    applyTheme();
    onCurrentImageChanged();
    rebuildRecentFilesMenu();
}

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    auto *openAction = fileMenu->addAction(QStringLiteral("打开(&O)..."),
                                           this, &MainWindow::openImages);
    openAction->setObjectName("openAction");
    openAction->setShortcut(QKeySequence::Open);  // Ctrl+O

    m_recentMenu = fileMenu->addMenu(QStringLiteral("打开最近"));

    fileMenu->addSeparator();

    m_saveAction = fileMenu->addAction(QStringLiteral("保存(&S)"),
                                       this, &MainWindow::saveImageAs);
    m_saveAction->setObjectName("saveAction");
    m_saveAction->setShortcut(QKeySequence::Save);  // Ctrl+S
    m_saveAction->setEnabled(false);

    m_reloadAction = fileMenu->addAction(QStringLiteral("重新加载(&R)"),
                                         this, &MainWindow::reloadCurrentImage);
    m_reloadAction->setObjectName("reloadAction");
    m_reloadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    m_reloadAction->setEnabled(false);

    m_closeAction = fileMenu->addAction(QStringLiteral("关闭当前(&W)"), this, [this] {
        AppState::instance()->closeImage(AppState::instance()->currentIndex());
    });
    m_closeAction->setObjectName("closeAction");
    m_closeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    m_closeAction->setEnabled(false);

    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出(&X)"), qApp, &QCoreApplication::quit);

    auto *editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));

    m_undoAction = editMenu->addAction(QStringLiteral("撤销(&U)"), this,
                                       [this] { m_viewer->undo(); });
    m_undoAction->setObjectName("undoAction");
    m_undoAction->setShortcut(QKeySequence::Undo);  // Ctrl+Z
    m_undoAction->setEnabled(false);

    m_redoAction = editMenu->addAction(QStringLiteral("重做(&Y)"), this,
                                       [this] { m_viewer->redo(); });
    m_redoAction->setObjectName("redoAction");
    m_redoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Y")));
    m_redoAction->setEnabled(false);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));

    // QDockWidget::toggleViewAction() keeps its check state in sync with
    // the dock's visibility, including closes via the title bar
    auto *panelToggle = m_panelDock->toggleViewAction();
    panelToggle->setText(QStringLiteral("面板"));
    panelToggle->setObjectName("panelDockToggle");
    panelToggle->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    viewMenu->addAction(panelToggle);

    viewMenu->addSeparator();

    auto *themeMenu = viewMenu->addMenu(QStringLiteral("主题"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    const QList<QPair<QString, QString>> themes{
        {QStringLiteral("跟随系统"), QStringLiteral("system")},
        {QStringLiteral("浅色"), QStringLiteral("light")},
        {QStringLiteral("深色"), QStringLiteral("dark")},
    };
    for (const auto &[text, value] : themes) {
        auto *action = themeMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(AppState::instance()->theme() == value);
        themeGroup->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, value] { AppState::instance()->setTheme(value); });
    }

    viewMenu->addSeparator();

    m_fitAction = viewMenu->addAction(QStringLiteral("适应窗口"), this, [this] {
        AppState::instance()->setZoomMode(AppState::Fit);
    });
    m_fitAction->setObjectName("fitAction");
    m_fitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
    m_fitAction->setEnabled(false);

    m_actualAction = viewMenu->addAction(QStringLiteral("实际大小"), this, [this] {
        AppState::instance()->setZoomMode(AppState::Actual);
    });
    m_actualAction->setObjectName("actualAction");
    m_actualAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+2")));
    m_actualAction->setEnabled(false);

    m_resetZoomAction = viewMenu->addAction(QStringLiteral("重置缩放"), this, [this] {
        AppState::instance()->setZoomMode(AppState::Fit);
    });
    m_resetZoomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    m_resetZoomAction->setEnabled(false);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于 Chameleon(&A)..."), this, [this] {
        QMessageBox::about(this, QStringLiteral("关于 Chameleon"),
                           QStringLiteral("<h3>Chameleon</h3>"
                                          "<p>基于 Qt Widgets + OpenCV 的图片处理软件</p>"
                                          "<p>版本 0.1.0</p>"));
    });
}

void MainWindow::setupToolBar()
{
    auto *toolBar = addToolBar(QStringLiteral("主工具栏"));
    toolBar->setMovable(false);

    toolBar->addAction(findChild<QAction *>("openAction"));
    toolBar->addAction(m_saveAction);
    toolBar->addAction(m_reloadAction);
    toolBar->addAction(m_undoAction);
    toolBar->addAction(m_redoAction);

    // Histogram tools live in one toolbar entry: display and equalization
    auto *histogramButton = new QToolButton(toolBar);
    histogramButton->setText(QStringLiteral("直方图"));
    histogramButton->setObjectName("histogramButton");
    histogramButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    histogramButton->setPopupMode(QToolButton::InstantPopup);
    auto *histogramMenu = new QMenu(histogramButton);
    histogramMenu->setObjectName("histogramMenu");
    histogramMenu->addAction(QStringLiteral("灰度直方图"), this, &MainWindow::showHistograms);
    histogramMenu->addAction(QStringLiteral("直方图均衡化"), this,
                             [this] { m_viewer->equalizeHistogram(); });
    histogramMenu->addAction(QStringLiteral("CLAHE 均衡化"), this,
                             [this] { m_viewer->equalizeClahe(); });
    histogramMenu->addSeparator();
    histogramMenu->addAction(QStringLiteral("直方图比较..."), this,
                             &MainWindow::compareHistograms);
    histogramMenu->addAction(QStringLiteral("直方图匹配..."), this,
                             &MainWindow::matchHistograms);
    histogramMenu->addAction(QStringLiteral("直方图反向投影..."), this,
                             &MainWindow::backProjectHistograms);
    histogramButton->setMenu(histogramMenu);
    toolBar->addWidget(histogramButton);

    auto *convolutionButton = new QToolButton(toolBar);
    convolutionButton->setText(QStringLiteral("卷积..."));
    convolutionButton->setObjectName("convolutionButton");
    convolutionButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(convolutionButton, &QToolButton::clicked,
            this, &MainWindow::openConvolutionDialog);
    toolBar->addWidget(convolutionButton);

    auto *gradientButton = new QToolButton(toolBar);
    gradientButton->setText(QStringLiteral("梯度..."));
    gradientButton->setObjectName("gradientButton");
    gradientButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(gradientButton, &QToolButton::clicked,
            this, &MainWindow::openGradientDialog);
    toolBar->addWidget(gradientButton);

    auto *edgeButton = new QToolButton(toolBar);
    edgeButton->setText(QStringLiteral("边缘..."));
    edgeButton->setObjectName("edgeButton");
    edgeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(edgeButton, &QToolButton::clicked,
            this, &MainWindow::openEdgeDialog);
    toolBar->addWidget(edgeButton);

    auto *noiseButton = new QToolButton(toolBar);
    noiseButton->setText(QStringLiteral("噪声..."));
    noiseButton->setObjectName("noiseButton");
    noiseButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(noiseButton, &QToolButton::clicked,
            this, &MainWindow::openNoiseDialog);
    toolBar->addWidget(noiseButton);

    auto *sharpenButton = new QToolButton(toolBar);
    sharpenButton->setText(QStringLiteral("锐化..."));
    sharpenButton->setObjectName("sharpenButton");
    sharpenButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(sharpenButton, &QToolButton::clicked,
            this, &MainWindow::openSharpenDialog);
    toolBar->addWidget(sharpenButton);

    auto *spacer = new QWidget(toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar->addWidget(spacer);

    m_fitAction->setCheckable(true);
    m_actualAction->setCheckable(true);
    toolBar->addAction(m_fitAction);
    toolBar->addAction(m_actualAction);

    m_toolbarZoom = new QLabel(QString(), toolBar);
    m_toolbarZoom->setFixedWidth(56);
    m_toolbarZoom->setAlignment(Qt::AlignCenter);
    toolBar->addWidget(m_toolbarZoom);
}

void MainWindow::setupDocks()
{
    m_thumbnailList = new QListWidget(this);
    m_thumbnailList->setObjectName("thumbnailList");
    m_thumbnailList->setIconSize(QSize(96, 72));
    connect(m_thumbnailList, &QListWidget::currentRowChanged, this,
            [this](int row) { AppState::instance()->setCurrentIndex(row); });

    // One side panel hosting the thumbnails and the filter controls as
    // tabs of a single dock
    m_filterPanel = new FilterPanel(this);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("panelTabs");
    tabs->addTab(m_thumbnailList, QStringLiteral("缩略图"));
    tabs->addTab(m_filterPanel, QStringLiteral("控制面板"));

    m_panelDock = new QDockWidget(QStringLiteral("面板"), this);
    m_panelDock->setObjectName("panelDock");
    m_panelDock->setWidget(tabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_panelDock);
}

void MainWindow::setupStatusBar()
{
    m_statusFileName = new QLabel(QString(), this);
    m_statusFileName->setObjectName("statusFileName");
    m_statusFileName->setMinimumWidth(220);
    m_statusSize = new QLabel(QString(), this);
    m_statusSize->setObjectName("statusSize");
    m_statusFormat = new QLabel(QString(), this);
    m_statusFormat->setObjectName("statusFormat");
    m_statusZoom = new QLabel(QString(), this);
    m_statusZoom->setObjectName("statusZoom");
    m_statusFileSize = new QLabel(QString(), this);
    m_statusFileSize->setObjectName("statusFileSize");

    statusBar()->addWidget(m_statusFileName);
    statusBar()->addWidget(m_statusSize);
    statusBar()->addWidget(m_statusFormat);
    statusBar()->addWidget(m_statusZoom);
    statusBar()->addWidget(m_statusFileSize);
}

void MainWindow::applyTheme()
{
    const QString theme = AppState::instance()->theme();
    bool dark = theme == QLatin1String("dark");
    if (theme == QLatin1String("system")) {
        QSettings registry(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows"
                           "\\CurrentVersion\\Themes\\Personalize"),
            QSettings::NativeFormat);
        dark = registry.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0;
    }
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setPalette(dark ? darkPalette() : qApp->style()->standardPalette());
}

void MainWindow::openImages()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("打开图片"), QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;"
                       "所有文件 (*.*)"));
    if (files.isEmpty())
        return;
    AppState::instance()->openImages(files);
}

QMenu *MainWindow::createViewerContextMenu()
{
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(m_fitAction);
    menu->addAction(m_actualAction);
    menu->addSeparator();

    menu->addAction(QStringLiteral("全选"), m_viewer, &ImageViewerWidget::selectAll);
    auto *clearAction = menu->addAction(QStringLiteral("清除选区"), m_viewer,
                                        &ImageViewerWidget::clearSelection);
    clearAction->setEnabled(m_viewer->hasSelection());

    menu->addSeparator();
    auto *saveSelectionAction = menu->addAction(QStringLiteral("保存选区为 PNG..."),
                                                this, &MainWindow::saveSelectionAs);
    saveSelectionAction->setEnabled(m_viewer->hasSelection());
    return menu;
}

void MainWindow::showViewerContextMenu(const QPoint &globalPos)
{
    createViewerContextMenu()->popup(globalPos);
}

void MainWindow::saveSelectionAs()
{
    const cv::Mat selection = m_viewer->selectionImage();
    if (selection.empty())
        return;
    const QString target = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存选区为 PNG"), QString(),
        QStringLiteral("PNG 图片 (*.png)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (target.isEmpty())
        return;
    QString path = target;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1String(".png");
    if (ImageIO::imwriteUtf8(path.toStdString(), selection))
        statusBar()->showMessage(
            QStringLiteral("已保存选区:%1").arg(QFileInfo(path).fileName()), 2500);
    else
        statusBar()->showMessage(QStringLiteral("保存选区失败"), 2500);
}

void MainWindow::showHistograms()
{
    // Selection first: histograms of the region of interest; otherwise of
    // the whole displayed image
    const cv::Mat source = m_viewer->hasSelection() ? m_viewer->selectionImage()
                                                    : m_viewer->currentImage();
    if (source.empty())
        return;

    // One popup window per channel: each channel is a grayscale map and
    // its histogram is the grayscale histogram of that channel
    if (source.channels() == 1) {
        if (auto *existing = focusExisting<HistogramWindow>(
                this, QStringLiteral("histogramWindow_gray"))) {
            existing->setBins(computeHistogram(source, 0));
            return;
        }
        auto *window = new HistogramWindow(QStringLiteral("灰度直方图"),
                                           QColor(140, 140, 140),
                                           computeHistogram(source, 0), this);
        window->setObjectName(QStringLiteral("histogramWindow_gray"));
        window->show();
        return;
    }

    struct ChannelInfo {
        const char *name;
        const char *objectName;
        QColor color;
    };
    const ChannelInfo channels[] = {
        {QT_TR_NOOP("蓝色直方图"), "histogramWindow_blue", QColor(60, 120, 255)},
        {QT_TR_NOOP("绿色直方图"), "histogramWindow_green", QColor(60, 190, 60)},
        {QT_TR_NOOP("红色直方图"), "histogramWindow_red", QColor(255, 90, 90)},
    };
    const int count = std::min(3, source.channels());
    for (int c = 0; c < count; ++c) {
        const QString objectName = QString::fromUtf8(channels[c].objectName);
        if (auto *existing = focusExisting<HistogramWindow>(this, objectName)) {
            existing->setBins(computeHistogram(source, c));
            continue;
        }
        auto *window = new HistogramWindow(QString::fromUtf8(channels[c].name),
                                           channels[c].color,
                                           computeHistogram(source, c), this);
        window->setObjectName(objectName);
        // Cascade so the per-channel windows do not fully overlap
        window->move(pos().x() + 60 + c * 40, pos().y() + 80 + c * 40);
        window->show();
    }
}

void MainWindow::compareHistograms()
{
    // Reference: the selection when one exists, else the whole displayed
    // image; targets: all opened images
    const bool useSelection = m_viewer->hasSelection();
    const cv::Mat reference = useSelection ? m_viewer->selectionImage()
                                           : m_viewer->currentImage();
    if (reference.empty())
        return;
    if (focusExisting<HistogramCompareDialog>(this))
        return;
    auto *dialog = new HistogramCompareDialog(
        reference,
        useSelection ? QStringLiteral("当前选区") : QStringLiteral("当前显示图"),
        AppState::instance()->images(), this);
    dialog->show();
}

void MainWindow::matchHistograms()
{
    if (m_viewer->currentImage().empty())
        return;
    if (focusExisting<HistogramMatchDialog>(this))
        return;
    auto *dialog = new HistogramMatchDialog(AppState::instance()->images(), this);
    connect(dialog, &HistogramMatchDialog::applyRequested, this, [this, dialog] {
        const cv::Mat templateImage = dialog->currentTemplateImage();
        if (templateImage.empty()) {
            statusBar()->showMessage(QStringLiteral("无法加载模板图片"), 2500);
            return;
        }
        m_viewer->matchHistogram(templateImage);
        statusBar()->showMessage(
            QStringLiteral("已应用直方图匹配:%1").arg(dialog->currentTemplateName()), 2500);
    });
    dialog->show();
}

void MainWindow::backProjectHistograms()
{
    // Target: the selection when one exists, else the whole displayed
    // image (state captured when the dialog opens)
    const cv::Mat target = m_viewer->hasSelection() ? m_viewer->selectionImage()
                                                    : m_viewer->currentImage();
    if (target.empty())
        return;
    if (focusExisting<BackProjectDialog>(this))
        return;
    auto *dialog = new BackProjectDialog(AppState::instance()->images(), this);
    connect(dialog, &BackProjectDialog::computeRequested, this, [this, dialog, target] {
        const cv::Mat templateImage = dialog->currentTemplateImage();
        if (templateImage.empty()) {
            statusBar()->showMessage(QStringLiteral("无法加载模板图片"), 2500);
            return;
        }
        const cv::Mat result = chameleon::backProjectHistogram(target, templateImage);
        if (result.empty())
            return;
        const QString title = QStringLiteral("反向投影结果:%1")
                                  .arg(dialog->currentTemplateName());
        if (auto *existing = focusExisting<ImageWindow>(
                this, QStringLiteral("backProjectionWindow"))) {
            existing->setImage(title, result);
        } else {
            auto *window = new ImageWindow(title, result, this);
            window->setObjectName(QStringLiteral("backProjectionWindow"));
            window->show();
        }
        statusBar()->showMessage(QStringLiteral("已生成反向投影结果"), 2500);
    });
    dialog->show();
}

void MainWindow::openConvolutionDialog()
{
    if (m_viewer->currentImage().empty())
        return;
    if (focusExisting<ConvolutionDialog>(this))
        return;
    auto *dialog = new ConvolutionDialog(this);
    connect(dialog, &ConvolutionDialog::applyRequested, this, [this, dialog] {
        m_viewer->applyConvolution(dialog->blurParams());
        statusBar()->showMessage(
            QStringLiteral("已应用卷积:%1").arg(dialog->currentTypeName()), 2500);
    });
    dialog->show();
}

void MainWindow::openGradientDialog()
{
    if (m_viewer->currentImage().empty())
        return;
    if (focusExisting<GradientDialog>(this))
        return;
    auto *dialog = new GradientDialog(this);
    connect(dialog, &GradientDialog::applyRequested, this, [this, dialog] {
        m_viewer->applyGradient(dialog->gradientParams());
        statusBar()->showMessage(
            QStringLiteral("已应用梯度:%1").arg(dialog->currentTypeName()), 2500);
    });
    dialog->show();
}

void MainWindow::openEdgeDialog()
{
    if (m_viewer->currentImage().empty())
        return;
    if (focusExisting<EdgeDialog>(this))
        return;
    auto *dialog = new EdgeDialog(this);
    connect(dialog, &EdgeDialog::applyRequested, this, [this, dialog] {
        m_viewer->applyEdgeDetection(dialog->cannyParams());
        statusBar()->showMessage(QStringLiteral("已应用边缘检测(Canny)"), 2500);
    });
    dialog->show();
}

void MainWindow::openNoiseDialog()
{
    if (m_viewer->currentImage().empty())
        return;
    if (focusExisting<NoiseDialog>(this))
        return;
    auto *dialog = new NoiseDialog(this);
    connect(dialog, &NoiseDialog::addNoiseRequested, this, [this, dialog] {
        if (dialog->saltPepperSelected())
            m_viewer->addSaltPepperNoise(dialog->saltPepperParams());
        else
            m_viewer->addGaussianNoise(dialog->gaussianNoiseParams());
        statusBar()->showMessage(QStringLiteral("已添加噪声"), 2500);
    });
    connect(dialog, &NoiseDialog::denoiseRequested, this, [this, dialog] {
        m_viewer->applyConvolution(dialog->denoiseParams());
        statusBar()->showMessage(QStringLiteral("已去噪"), 2500);
    });
    dialog->show();
}

void MainWindow::openSharpenDialog()
{
    if (m_viewer->currentImage().empty())
        return;
    if (focusExisting<SharpenDialog>(this))
        return;
    auto *dialog = new SharpenDialog(this);

    // Sharpness of the current image or selection, refreshed on demand
    // and after every apply
    auto updateSharpness = [this, dialog] {
        const cv::Mat source = m_viewer->hasSelection() ? m_viewer->selectionImage()
                                                        : m_viewer->currentImage();
        dialog->setSharpness(chameleon::imageSharpness(source));
    };
    connect(dialog, &SharpenDialog::applyRequested, this, [this, dialog, updateSharpness] {
        m_viewer->applySharpening(dialog->sharpenParams());
        updateSharpness();
        statusBar()->showMessage(QStringLiteral("已应用拉普拉斯锐化"), 2500);
    });
    connect(dialog, &SharpenDialog::measureRequested, this, updateSharpness);
    updateSharpness();
    dialog->show();
}

void MainWindow::reloadCurrentImage()
{
    const QString path = AppState::instance()->currentImage();
    if (path.isEmpty())
        return;
    // Back to the initial view state: Fit zoom, pan reset, re-decode;
    // processing edits are discarded on both sides
    AppState::instance()->setZoomMode(AppState::Fit);
    m_viewer->reloadImage();
    m_originalViewer->setImage(path);
}

void MainWindow::saveImageAs()
{
    const QString target = QFileDialog::getSaveFileName(
        this, QStringLiteral("另存为 PNG"), QString(),
        QStringLiteral("PNG 图片 (*.png)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (target.isEmpty())
        return;
    QString path = target;
    if (QFileInfo(path).suffix().isEmpty())
        path += QLatin1String(".png");
    if (m_viewer->saveAs(path))
        statusBar()->showMessage(
            QStringLiteral("已保存:%1").arg(QFileInfo(path).fileName()), 2500);
    else
        statusBar()->showMessage(QStringLiteral("保存失败"), 2500);
}

void MainWindow::rebuildThumbnails()
{
    // Closed images must not keep remembered edits
    m_viewer->retainRememberedStates(AppState::instance()->images());
    m_thumbnailList->clear();
    const QStringList images = AppState::instance()->images();
    for (int i = 0; i < images.size(); ++i) {
        auto *item = new QListWidgetItem(m_thumbnailList);
        item->setSizeHint(QSize(0, 84));
        auto *widget = new ThumbnailItemWidget(
            i, images.at(i),
            [this](int index) { AppState::instance()->setCurrentIndex(index); },
            [this](int index) { AppState::instance()->closeImage(index); },
            m_thumbnailList);
        m_thumbnailList->setItemWidget(item, widget);
    }
    if (AppState::instance()->hasImage())
        m_thumbnailList->setCurrentRow(AppState::instance()->currentIndex());
}

void MainWindow::rebuildRecentFilesMenu()
{
    m_recentMenu->clear();
    const QStringList recent = AppState::instance()->recentFiles();
    m_recentMenu->setEnabled(!recent.isEmpty());
    for (const QString &path : recent) {
        m_recentMenu->addAction(QFileInfo(path).fileName(), this, [this, path] {
            AppState::instance()->openImages({path});
        });
    }
}

void MainWindow::updateStatusBar()
{
    const auto *state = AppState::instance();
    if (!state->hasImage()) {
        m_statusFileName->setText(QStringLiteral("未打开图片"));
        m_statusSize->clear();
        m_statusFormat->clear();
        m_statusZoom->clear();
        m_statusFileSize->clear();
        return;
    }
    const QString path = state->currentImage();
    m_statusFileName->setText(QFileInfo(path).fileName());
    const QSize size = state->imageInfo();
    if (size.isValid())
        m_statusSize->setText(QStringLiteral("%1 × %2").arg(size.width()).arg(size.height()));
    else
        m_statusSize->clear();
    m_statusFormat->setText(QFileInfo(path).suffix().toUpper());
    chameleon::FileUtils fileUtils;
    m_statusFileSize->setText(formatSize(fileUtils.fileSizeOf(QUrl::fromLocalFile(path))));
}

void MainWindow::updateZoomUi()
{
    const auto *state = AppState::instance();
    if (!state->hasImage()) {
        m_statusZoom->clear();
        m_toolbarZoom->clear();
        m_fitAction->setChecked(false);
        m_actualAction->setChecked(false);
        return;
    }
    switch (state->zoomMode()) {
    case AppState::Fit:
        m_statusZoom->setText(QStringLiteral("适应窗口"));
        m_toolbarZoom->setText(QStringLiteral("适应"));
        m_fitAction->setChecked(true);
        m_actualAction->setChecked(false);
        break;
    case AppState::Actual:
        m_statusZoom->setText(QStringLiteral("100%"));
        m_toolbarZoom->setText(QStringLiteral("100%"));
        m_fitAction->setChecked(false);
        m_actualAction->setChecked(true);
        break;
    case AppState::Custom:
        m_statusZoom->setText(QStringLiteral("%1%").arg(
            qRound(state->zoomFactorForCurrent() * 100)));
        m_toolbarZoom->setText(m_statusZoom->text());
        m_fitAction->setChecked(false);
        m_actualAction->setChecked(false);
        break;
    }
}

void MainWindow::onCurrentImageChanged()
{
    const QString path = AppState::instance()->currentImage();
    // Only (re)load when the displayed image actually changed; closing a
    // background tab shifts indexes but must not reload the current image
    if (path != m_shownImagePath) {
        m_viewer->setImage(path);
        m_originalViewer->setImage(path);
        m_shownImagePath = path;
    }
    const bool hasImage = AppState::instance()->hasImage();
    m_saveAction->setEnabled(hasImage);
    m_reloadAction->setEnabled(hasImage);
    m_closeAction->setEnabled(hasImage);
    m_fitAction->setEnabled(hasImage);
    m_actualAction->setEnabled(hasImage);
    m_resetZoomAction->setEnabled(hasImage);
    updateStatusBar();
    updateZoomUi();
}

} // namespace chameleon

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
#include <QStyleFactory>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <utility>

#include "app/app_state.h"
#include "ui/filter_panel.h"
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
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QStringLiteral("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
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
        QPixmap pixmap(path);
        thumb->setPixmap(pixmap.scaled(96, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    setupMenus();
    setupToolBar();
    setupDocks();
    setupStatusBar();

    m_viewer = new ImageViewerWidget(this);
    m_viewer->setObjectName("imageViewer");
    setCentralWidget(m_viewer);

    auto *state = AppState::instance();
    connect(state, &AppState::currentIndexChanged, this, &MainWindow::onCurrentImageChanged);
    connect(state, &AppState::imagesChanged, this, &MainWindow::rebuildThumbnails);
    connect(state, &AppState::zoomChanged, this, &MainWindow::updateZoomUi);
    connect(state, &AppState::imageInfoChanged, this, &MainWindow::updateStatusBar);
    connect(state, &AppState::recentFilesChanged, this, &MainWindow::rebuildRecentFilesMenu);
    connect(state, &AppState::themeChanged, this, [this] { applyTheme(); });

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

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));

    auto *leftToggle = viewMenu->addAction(QStringLiteral("缩略图栏"));
    leftToggle->setCheckable(true);
    leftToggle->setChecked(true);
    leftToggle->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    connect(leftToggle, &QAction::toggled, this,
            [this](bool on) { m_leftDock->setVisible(on); });

    auto *rightToggle = viewMenu->addAction(QStringLiteral("侧边栏"));
    rightToggle->setCheckable(true);
    rightToggle->setChecked(true);
    rightToggle->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    connect(rightToggle, &QAction::toggled, this,
            [this](bool on) { m_rightDock->setVisible(on); });

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
    toolBar->addAction(QStringLiteral("撤销"))->setEnabled(false);
    toolBar->addAction(QStringLiteral("重做"))->setEnabled(false);

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

    m_leftDock = new QDockWidget(QStringLiteral("缩略图"), this);
    m_leftDock->setObjectName("leftDock");
    m_leftDock->setWidget(m_thumbnailList);
    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);

    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(new FilterPanel(rightPanel));

    m_rightDock = new QDockWidget(QStringLiteral("面板"), this);
    m_rightDock->setObjectName("rightDock");
    m_rightDock->setWidget(rightPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);
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
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp *.gif *.tif *.tiff);;"
                       "所有文件 (*.*)"));
    if (files.isEmpty())
        return;
    AppState::instance()->openImages(files);
}

void MainWindow::reloadCurrentImage()
{
    const QString path = AppState::instance()->currentImage();
    if (path.isEmpty())
        return;
    // Back to the initial view state: Fit zoom, pan reset, re-decode
    AppState::instance()->setZoomMode(AppState::Fit);
    m_viewer->setImage(path);
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

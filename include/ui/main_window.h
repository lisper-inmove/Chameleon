#pragma once

#include <QMainWindow>

class QLabel;
class QListWidget;
class QMenu;

namespace chameleon {

class ImageViewerWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void applyTheme();

private:
    void setupMenus();
    void setupToolBar();
    void setupDocks();
    void setupStatusBar();
    void openImages();
    void saveImageAs();
    void rebuildThumbnails();
    void rebuildRecentFilesMenu();
    void updateStatusBar();
    void updateZoomUi();
    void onCurrentImageChanged();

    ImageViewerWidget *m_viewer = nullptr;
    QListWidget *m_thumbnailList = nullptr;
    QDockWidget *m_leftDock = nullptr;
    QDockWidget *m_rightDock = nullptr;
    QLabel *m_statusFileName = nullptr;
    QLabel *m_statusSize = nullptr;
    QLabel *m_statusFormat = nullptr;
    QLabel *m_statusZoom = nullptr;
    QLabel *m_statusFileSize = nullptr;
    QLabel *m_toolbarZoom = nullptr;
    QString m_shownImagePath;
    QAction *m_saveAction = nullptr;
    QAction *m_closeAction = nullptr;
    QAction *m_fitAction = nullptr;
    QAction *m_actualAction = nullptr;
    QAction *m_resetZoomAction = nullptr;
    QMenu *m_recentMenu = nullptr;
};

} // namespace chameleon

#include "app/app_state.h"

#include <QSettings>

namespace chameleon {

AppState *AppState::instance()
{
    static AppState s_instance;
    return &s_instance;
}

AppState::AppState(QObject *parent) : QObject(parent)
{
    QSettings settings;
    m_theme = settings.value("ui/theme", "system").toString();
    m_recentFiles = settings.value("ui/recentFiles").toStringList();
}

QString AppState::currentImage() const
{
    return hasImage() ? m_images.at(m_currentIndex) : QString();
}

bool AppState::hasImage() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_images.size();
}

void AppState::setCurrentIndex(int index)
{
    if (m_images.isEmpty())
        return;
    // Negative indexes are rejected: QListWidget::clear() emits
    // currentRowChanged(-1) and must not reset the selection to 0
    if (index < 0)
        return;
    const int clamped = qBound(0, index, m_images.size() - 1);
    if (clamped == m_currentIndex)
        return;
    m_currentIndex = clamped;
    emit currentIndexChanged(m_currentIndex);
}

double AppState::zoomFactorForCurrent() const
{
    return m_zoomByImage.value(currentZoomKey(), 1.0);
}

void AppState::setImageInfo(const QSize &size)
{
    if (m_imageInfo == size)
        return;
    m_imageInfo = size;
    emit imageInfoChanged();
}

void AppState::setTheme(const QString &theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    QSettings().setValue("ui/theme", m_theme);
    emit themeChanged();
}

void AppState::openImages(const QStringList &paths)
{
    if (paths.isEmpty())
        return;
    const int firstNew = m_images.size();
    m_images.append(paths);
    m_currentIndex = firstNew;
    for (const QString &path : paths) {
        m_recentFiles.removeAll(path);
        m_recentFiles.prepend(path);
    }
    while (m_recentFiles.size() > 8)
        m_recentFiles.removeLast();
    QSettings().setValue("ui/recentFiles", m_recentFiles);
    emit imagesChanged();
    emit currentIndexChanged(m_currentIndex);
    emit recentFilesChanged();
}

void AppState::closeImage(int index)
{
    if (index < 0 || index >= m_images.size())
        return;
    m_images.removeAt(index);
    if (m_images.isEmpty()) {
        m_currentIndex = -1;
        m_imageInfo = QSize();
    } else if (m_currentIndex > index) {
        --m_currentIndex;
    } else if (m_currentIndex >= m_images.size()) {
        m_currentIndex = m_images.size() - 1;
    }
    emit imagesChanged();
    emit currentIndexChanged(m_currentIndex);
    emit imageInfoChanged();
}

void AppState::setZoomMode(ZoomMode mode)
{
    m_zoomMode = mode;
    const QString key = currentZoomKey();
    if (mode == Fit) {
        if (!key.isEmpty())
            m_zoomByImage.remove(key);
    } else if (mode == Custom) {
        m_zoomFactor = zoomFactorForCurrent();
    } else if (mode == Actual && !key.isEmpty()) {
        m_zoomByImage[key] = 1.0;
    }
    emit zoomChanged();
}

void AppState::setZoomFactor(double factor)
{
    m_zoomFactor = qBound(0.1, factor, 8.0);
    m_zoomMode = Custom;
    const QString key = currentZoomKey();
    if (!key.isEmpty())
        m_zoomByImage[key] = m_zoomFactor;
    emit zoomChanged();
}

QString AppState::currentZoomKey() const
{
    return hasImage() ? m_images.at(m_currentIndex) : QString();
}

} // namespace chameleon

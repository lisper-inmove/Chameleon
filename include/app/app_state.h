#pragma once

#include <QHash>
#include <QObject>
#include <QSize>
#include <QStringList>

namespace chameleon {

// Central UI state: opened images, current tab, zoom, theme, recent files.
class AppState : public QObject
{
    Q_OBJECT
public:
    enum ZoomMode { Fit = 0, Actual = 1, Custom = 2 };
    Q_ENUM(ZoomMode)

    static AppState *instance();

    const QStringList &images() const { return m_images; }
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    QString currentImage() const;
    bool hasImage() const;

    ZoomMode zoomMode() const { return m_zoomMode; }
    double zoomFactor() const { return m_zoomFactor; }
    double zoomFactorForCurrent() const;

    QSize imageInfo() const { return m_imageInfo; }
    void setImageInfo(const QSize &size);

    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);

    const QStringList &recentFiles() const { return m_recentFiles; }

    void openImages(const QStringList &paths);
    void closeImage(int index);
    void setZoomMode(ZoomMode mode);
    void setZoomFactor(double factor);

signals:
    void imagesChanged();
    void currentIndexChanged(int index);
    void zoomChanged();
    void imageInfoChanged();
    void themeChanged();
    void recentFilesChanged();

private:
    explicit AppState(QObject *parent = nullptr);

    QString currentZoomKey() const;

    QStringList m_images;
    int m_currentIndex = -1;
    ZoomMode m_zoomMode = Fit;
    double m_zoomFactor = 1.0;
    QHash<QString, double> m_zoomByImage;
    QSize m_imageInfo;
    QString m_theme = "system";
    QStringList m_recentFiles;
};

} // namespace chameleon

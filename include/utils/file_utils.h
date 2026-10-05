#pragma once

#include <QObject>
#include <QUrl>

namespace chameleon {

// Small file helpers exposed to QML (injected as context property "fileUtils").
class FileUtils : public QObject
{
    Q_OBJECT
public:
    explicit FileUtils(QObject *parent = nullptr);

    // Returns the size of a local file in bytes, or -1 if unavailable.
    Q_INVOKABLE qint64 fileSizeOf(const QUrl &url) const;

    // Converts a file:// URL to a native local path (empty if not local).
    Q_INVOKABLE QString toLocalPath(const QUrl &url) const;
};

} // namespace chameleon

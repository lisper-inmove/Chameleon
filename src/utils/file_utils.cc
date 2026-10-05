#include "utils/file_utils.h"

#include <QFileInfo>

namespace chameleon {

FileUtils::FileUtils(QObject *parent) : QObject(parent) {}

qint64 FileUtils::fileSizeOf(const QUrl &url) const
{
    const QFileInfo info(url.toLocalFile());
    return info.exists() && info.isFile() ? info.size() : -1;
}

QString FileUtils::toLocalPath(const QUrl &url) const
{
    return url.toLocalFile();
}

} // namespace chameleon

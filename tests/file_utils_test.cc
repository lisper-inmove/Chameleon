#include <gtest/gtest.h>

#include <QFile>
#include <QUrl>

#include "utils/file_utils.h"

TEST(FileUtilsTest, FileSizeOfExistingFile)
{
    chameleon::FileUtils utils;
    const QUrl url = QUrl::fromLocalFile(TEST_DATA_DIR "/app.yaml");

    QFile file(url.toLocalFile());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const qint64 expected = file.size();
    file.close();

    EXPECT_EQ(utils.fileSizeOf(url), expected);
}

TEST(FileUtilsTest, FileSizeOfMissingFileReturnsMinusOne)
{
    chameleon::FileUtils utils;
    EXPECT_EQ(utils.fileSizeOf(QUrl::fromLocalFile("F:/no/such/file.png")), -1);
}

TEST(FileUtilsTest, ToLocalPathConvertsUrl)
{
    chameleon::FileUtils utils;
    EXPECT_EQ(utils.toLocalPath(QUrl::fromLocalFile("F:/images/test.png")).toStdString(),
              "F:/images/test.png");
}

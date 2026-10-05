#include <QApplication>
#include <QStandardPaths>
#include <gtest/gtest.h>

int main(int argc, char **argv)
{
    // UI tests render headlessly; must be set before QApplication
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Chameleon");
    QCoreApplication::setApplicationName("Chameleon");

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

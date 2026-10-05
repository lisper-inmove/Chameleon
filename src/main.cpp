#include <QApplication>

#include <opencv2/core.hpp>
#include <spdlog/spdlog.h>

#include "chameleon/config_paths.h"
#include "config/config_manager.h"
#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Chameleon");
    QCoreApplication::setApplicationName("Chameleon");

    spdlog::info("Chameleon starting, OpenCV {}", cv::getVersionString());

    chameleon::ConfigManager config;
    // Source tree first (dev), deployed copy next (next to the executable)
    if (!config.load(CHAMELEON_CONFIG_SOURCE_DIR "/app.yaml")
            && !config.load("configs/app.yaml"))
        spdlog::warn("Failed to load config, using defaults");
    spdlog::info("Loaded config: title={}, window={}x{}",
                 config.title(), config.windowWidth(), config.windowHeight());

    chameleon::MainWindow window;
    window.setWindowTitle(QString::fromStdString(config.title()));
    window.resize(config.windowWidth(), config.windowHeight());
    window.show();

    return app.exec();
}

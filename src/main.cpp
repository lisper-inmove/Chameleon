#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <opencv2/core.hpp>
#include <spdlog/spdlog.h>

#include "chameleon/config_paths.h"
#include "config/config_manager.h"
#include "utils/file_utils.h"

int main(int argc, char *argv[])
{
    spdlog::info("Chameleon starting, OpenCV {}", cv::getVersionString());

    QGuiApplication app(argc, argv);

    QQuickStyle::setStyle("Material");

    chameleon::ConfigManager config;
    // Source tree first (dev), deployed copy next (next to the executable)
    if (!config.load(CHAMELEON_CONFIG_SOURCE_DIR "/app.yaml")
            && !config.load("configs/app.yaml"))
        spdlog::warn("Failed to load config, using defaults");
    spdlog::info("Loaded config: title={}, window={}x{}",
                 config.title(), config.windowWidth(), config.windowHeight());

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // Expose the config through a context property; setInitialProperties()
    // does not reliably reach QML when the module is statically linked.
    chameleon::FileUtils fileUtils;
    engine.rootContext()->setContextProperty("fileUtils", &fileUtils);

    engine.rootContext()->setContextProperty("appConfig", QVariantMap{
        {"title", QString::fromStdString(config.title())},
        {"width", config.windowWidth()},
        {"height", config.windowHeight()},
    });

    engine.loadFromModule("Chameleon", "Main");

    return app.exec();
}

#include <gtest/gtest.h>

#include "config_manager.h"

TEST(ConfigManagerTest, LoadsValidFile)
{
    chameleon::ConfigManager config;
    ASSERT_TRUE(config.load(TEST_DATA_DIR "/app.yaml"));
    EXPECT_EQ(config.title(), "Chameleon");
    EXPECT_EQ(config.windowWidth(), 1600);
    EXPECT_EQ(config.windowHeight(), 1200);
}

TEST(ConfigManagerTest, MissingFileFallsBackToDefaults)
{
    chameleon::ConfigManager config;
    EXPECT_FALSE(config.load("does/not/exist.yaml"));
    EXPECT_EQ(config.title(), "Chameleon");
    EXPECT_EQ(config.windowWidth(), 640);
    EXPECT_EQ(config.windowHeight(), 480);
}

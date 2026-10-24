#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "rendering/opengl_image.h"

using chameleon::renderImageOffscreen;

TEST(OpenGLTest, RendersASolidImageOffscreen)
{
    const cv::Mat image(8, 8, CV_8UC3, cv::Scalar(0, 0, 255));  // BGR red
    const auto result = renderImageOffscreen(image, 16, 16);
    if (!result.success)
        GTEST_SKIP() << "OpenGL context unavailable: " << result.error;

    ASSERT_EQ(result.width, 16);
    ASSERT_EQ(result.height, 16);
    ASSERT_EQ(result.rgba.size(), size_t(16 * 16 * 4));

    const size_t center = (size_t(8) * 16 + 8) * 4;
    EXPECT_GT(result.rgba[center + 0], 200);  // R
    EXPECT_LT(result.rgba[center + 1], 60);   // G
    EXPECT_LT(result.rgba[center + 2], 60);   // B
    EXPECT_EQ(result.rgba[center + 3], 255);  // A
}

TEST(OpenGLTest, RendersWithTopDownOrientation)
{
    cv::Mat image(2, 2, CV_8UC3, cv::Scalar(0, 0, 255));  // red
    image.row(0).setTo(cv::Scalar(0, 255, 0));            // top row green
    const auto result = renderImageOffscreen(image, 16, 16);
    if (!result.success)
        GTEST_SKIP() << "OpenGL context unavailable: " << result.error;

    const auto pixel = [&result](int x, int y) {
        const size_t index = (size_t(y) * 16 + size_t(x)) * 4;
        return &result.rgba[index];
    };
    EXPECT_GT(pixel(8, 4)[1], 200);   // top: green
    EXPECT_GT(pixel(8, 12)[0], 200);  // bottom: red
}

TEST(OpenGLTest, RejectsUnsupportedInput)
{
    EXPECT_FALSE(renderImageOffscreen(cv::Mat(), 8, 8).success);

    const cv::Mat wrongDepth(4, 4, CV_32FC3, cv::Scalar::all(0.5));
    EXPECT_FALSE(renderImageOffscreen(wrongDepth, 8, 8).success);
}

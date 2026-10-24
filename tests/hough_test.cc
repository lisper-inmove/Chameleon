#include <gtest/gtest.h>

#include <cmath>

#include <opencv2/imgproc.hpp>

#include "processing/hough.h"

using chameleon::houghCircles;
using chameleon::houghLines;
using chameleon::HoughCirclesParams;
using chameleon::HoughLinesParams;

namespace {

// 200x200 black image with a vertical white stroke at x = 100
cv::Mat verticalLineImage(int channels = 1)
{
    cv::Mat image(200, 200, CV_8UC(channels), cv::Scalar::all(0));
    const cv::Scalar white =
        channels == 1 ? cv::Scalar(255) : cv::Scalar::all(255);
    cv::line(image, cv::Point(100, 20), cv::Point(100, 180), white, 3);
    return image;
}

cv::Mat ringImage()
{
    cv::Mat image(200, 200, CV_8UC1, cv::Scalar(0));
    cv::circle(image, cv::Point(100, 100), 30, cv::Scalar(255), 3);
    return image;
}

} // namespace

TEST(HoughTest, HoughLinesDetectsAKnownVerticalLine)
{
    const cv::Mat image = verticalLineImage();
    HoughLinesParams params;
    params.threshold = 100;
    const auto result = houghLines(image, params);

    ASSERT_FALSE(result.lines.empty());
    bool found = false;
    for (const auto &line : result.lines) {
        // A vertical line's normal points along x: theta ~ 0, rho ~ 100
        if (std::abs(line.rho - 100.0) < 3.0
                && (std::abs(line.thetaDegrees) < 3.0
                    || std::abs(line.thetaDegrees - 180.0) < 3.0)) {
            found = true;
        }
    }
    EXPECT_TRUE(found);

    // Detected lines are drawn across the whole image
    ASSERT_EQ(result.image.size(), image.size());
    cv::Mat diff;
    cv::absdiff(result.image, image, diff);
    EXPECT_GT(cv::countNonZero(diff), 0);
}

TEST(HoughTest, HoughLinesThresholdControlsTheCount)
{
    const cv::Mat image = verticalLineImage();
    HoughLinesParams params;
    params.threshold = 50;
    const auto many = houghLines(image, params);
    params.threshold = 100000;
    const auto none = houghLines(image, params);

    EXPECT_GT(many.lines.size(), none.lines.size());
    EXPECT_TRUE(none.lines.empty());
}

TEST(HoughTest, HoughCirclesDetectsAKnownCircle)
{
    const cv::Mat image = ringImage();
    HoughCirclesParams params;
    params.dp = 1.0;
    params.minDist = 50.0;
    params.param1 = 100.0;
    params.param2 = 20.0;
    params.minRadius = 20;
    params.maxRadius = 40;
    const auto result = houghCircles(image, params);

    ASSERT_FALSE(result.circles.empty());
    bool found = false;
    for (const auto &circle : result.circles) {
        if (std::abs(circle.center.x - 100.0f) < 4.0f
                && std::abs(circle.center.y - 100.0f) < 4.0f
                && std::abs(circle.radius - 30.0f) < 4.0f) {
            found = true;
        }
    }
    EXPECT_TRUE(found);

    cv::Mat diff;
    cv::absdiff(result.image, image, diff);
    EXPECT_GT(cv::countNonZero(diff), 0);
}

TEST(HoughTest, HoughCirclesRespectsTheRadiusRange)
{
    const cv::Mat image = ringImage();
    HoughCirclesParams params;
    params.dp = 1.0;
    params.minDist = 50.0;
    params.param1 = 100.0;
    params.param2 = 20.0;
    params.minRadius = 40;  // the real ring (r ~ 30) is outside the range
    params.maxRadius = 60;
    const auto result = houghCircles(image, params);

    for (const auto &circle : result.circles)
        EXPECT_GE(circle.radius, 40.0f);
}

TEST(HoughTest, HoughPreservesTheImageType)
{
    const cv::Mat color = verticalLineImage(3);
    HoughLinesParams params;
    params.threshold = 100;
    const auto result = houghLines(color, params);
    EXPECT_EQ(result.image.type(), color.type());
    EXPECT_EQ(result.image.size(), color.size());
    EXPECT_FALSE(result.lines.empty());

    HoughCirclesParams circleParams;
    circleParams.param2 = 20.0;
    const auto circles = houghCircles(color, circleParams);
    EXPECT_EQ(circles.image.type(), color.type());
}

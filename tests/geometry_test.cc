#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include "processing/geometry.h"

using chameleon::adjustCanvas;
using chameleon::FlipDirection;
using chameleon::flipImage;
using chameleon::perspectiveCorrect;
using chameleon::ResampleMethod;
using chameleon::resizeImage;
using chameleon::rotateImage;

namespace {

// 16 rows x 32 cols, value = x * 8
cv::Mat horizontalGradient()
{
    cv::Mat image(16, 32, CV_8UC1);
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x)
            image.at<uchar>(y, x) = uchar(x * 8);
    }
    return image;
}

} // namespace

TEST(GeometryTest, Rotate90MultiplesAreExact)
{
    const cv::Mat source = horizontalGradient();  // H=16, W=32

    // Positive angles are counter-clockwise, matching getRotationMatrix2D
    // (the arbitrary-angle path)
    const cv::Mat ccw = rotateImage(source, 90);
    ASSERT_EQ(ccw.rows, 32);
    ASSERT_EQ(ccw.cols, 16);
    // CCW: src(y, x) -> dst(W-1-x, y)
    EXPECT_EQ(ccw.at<uchar>(31, 0), source.at<uchar>(0, 0));
    EXPECT_EQ(ccw.at<uchar>(0, 0), source.at<uchar>(0, 31));
    EXPECT_EQ(ccw.at<uchar>(31, 15), source.at<uchar>(15, 0));

    const cv::Mat half = rotateImage(source, 180);
    ASSERT_EQ(half.size(), source.size());
    EXPECT_EQ(half.at<uchar>(0, 0), source.at<uchar>(15, 31));
    EXPECT_EQ(half.at<uchar>(15, 31), source.at<uchar>(0, 0));

    const cv::Mat cw = rotateImage(source, 270);
    ASSERT_EQ(cw.rows, 32);
    ASSERT_EQ(cw.cols, 16);
    // CW: src(y, x) -> dst(x, H-1-y)
    EXPECT_EQ(cw.at<uchar>(0, 15), source.at<uchar>(0, 0));
    EXPECT_EQ(cw.at<uchar>(0, 0), source.at<uchar>(15, 0));
}

TEST(GeometryTest, Rotate90ThenMinus90IsAnExactInverse)
{
    const cv::Mat source = horizontalGradient();

    const cv::Mat rotated = rotateImage(source, 90);
    const cv::Mat restored = rotateImage(rotated, -90);
    ASSERT_EQ(restored.size(), source.size());
    cv::Mat diff;
    cv::absdiff(restored, source, diff);
    EXPECT_EQ(cv::countNonZero(diff), 0);
}

TEST(GeometryTest, RotateArbitraryAngleExpandsTheCanvas)
{
    const cv::Mat uniform(16, 32, CV_8UC1, cv::Scalar(100));

    const cv::Mat out = rotateImage(uniform, 45);
    EXPECT_GT(out.rows, 16);
    EXPECT_GT(out.cols, 16);
    EXPECT_NE(out.size(), uniform.size());
    // The interior stays the uniform value; the expanded corners are black
    EXPECT_EQ(out.at<uchar>(out.rows / 2, out.cols / 2), 100);
    EXPECT_EQ(out.at<uchar>(0, 0), 0);
}

TEST(GeometryTest, FlipDirections)
{
    const cv::Mat source = horizontalGradient();

    const cv::Mat horizontal = flipImage(source, FlipDirection::Horizontal);
    EXPECT_EQ(horizontal.at<uchar>(0, 0), source.at<uchar>(0, 31));

    const cv::Mat vertical = flipImage(source, FlipDirection::Vertical);
    EXPECT_EQ(vertical.at<uchar>(0, 0), source.at<uchar>(15, 0));

    const cv::Mat both = flipImage(source, FlipDirection::Both);
    EXPECT_EQ(both.at<uchar>(0, 0), source.at<uchar>(15, 31));
}

TEST(GeometryTest, ResizeMethodsAndInterpolation)
{
    cv::Mat small(4, 4, CV_8UC1);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x)
            small.at<uchar>(y, x) = uchar(x * 60 + y * 10);
    }

    const cv::Mat nearest =
        resizeImage(small, 8, 8, ResampleMethod::Nearest);
    const cv::Mat linear =
        resizeImage(small, 8, 8, ResampleMethod::Linear);
    const cv::Mat cubic =
        resizeImage(small, 8, 8, ResampleMethod::Cubic);
    EXPECT_EQ(nearest.size(), cv::Size(8, 8));
    EXPECT_EQ(linear.size(), cv::Size(8, 8));
    EXPECT_EQ(cubic.size(), cv::Size(8, 8));

    // Nearest reproduces source values exactly; linear introduces new ones
    cv::Mat diff;
    cv::absdiff(nearest, linear, diff);
    EXPECT_GT(cv::countNonZero(diff), 0);
    EXPECT_EQ(nearest.at<uchar>(0, 0), small.at<uchar>(0, 0));
}

TEST(GeometryTest, PerspectiveCorrectMapsTheQuadToARectangle)
{
    const cv::Mat uniform(32, 32, CV_8UC1, cv::Scalar(100));
    std::array<cv::Point2f, 4> quad{
        cv::Point2f(4, 2), cv::Point2f(28, 4), cv::Point2f(30, 30), cv::Point2f(2, 28)};

    const cv::Mat out = perspectiveCorrect(uniform, quad);
    ASSERT_FALSE(out.empty());
    // Output size = the longest horizontal / vertical edges of the quad:
    // w = max(|p1-p0|, |p2-p3|) = |(28, 2)| ~ 28, h = |(2, 26)| ~ 26
    EXPECT_EQ(out.cols, 28);
    EXPECT_EQ(out.rows, 26);
    EXPECT_EQ(out.at<uchar>(out.rows / 2, out.cols / 2), 100);

    // Mapping the full source corners back gives the original size
    std::array<cv::Point2f, 4> full{cv::Point2f(0, 0), cv::Point2f(31, 0),
                                    cv::Point2f(31, 15), cv::Point2f(0, 15)};
    const cv::Mat identity = perspectiveCorrect(horizontalGradient(), full);
    EXPECT_EQ(identity.size(), cv::Size(31, 15));  // w-1 / h-1 edges
}

TEST(GeometryTest, AdjustCanvasPadsAndCrops)
{
    const cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));

    const cv::Mat padded = adjustCanvas(gray, 24, 20);
    ASSERT_EQ(padded.size(), cv::Size(24, 20));
    EXPECT_EQ(padded.at<uchar>(0, 0), 100);    // original kept top-left
    EXPECT_EQ(padded.at<uchar>(19, 23), 0);    // padding black

    const cv::Mat cropped = adjustCanvas(gray, 8, 8);
    ASSERT_EQ(cropped.size(), cv::Size(8, 8));
    EXPECT_EQ(cropped.at<uchar>(7, 7), 100);
}

TEST(GeometryTest, RotatePreservesChannels)
{
    const cv::Mat bgra(16, 32, CV_8UC4, cv::Scalar(30, 120, 200, 77));
    const cv::Mat out = rotateImage(bgra, 90);
    ASSERT_EQ(out.type(), CV_8UC4);
    EXPECT_EQ(out.at<cv::Vec4b>(3, 3)[3], 77);
}

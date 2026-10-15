#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/filter_processor.h"

using chameleon::applyBlur;
using chameleon::BlurParams;
using chameleon::BlurType;
using chameleon::FilterProcessor;

TEST(FilterProcessorTest, IdentityWhenAllParamsZero)
{
    const cv::Mat src(64, 64, CV_8UC3, cv::Scalar(30, 120, 200));

    const cv::Mat out = FilterProcessor::apply(src, 0, 0, 0);
    ASSERT_EQ(out.size(), src.size());
    cv::Mat diff;
    cv::absdiff(out, src, diff);
    EXPECT_EQ(cv::countNonZero(diff.reshape(1)), 0);
}

TEST(FilterProcessorTest, PositiveBrightnessIncreasesEveryPixel)
{
    const cv::Mat src(32, 32, CV_8UC3, cv::Scalar(100, 100, 100));

    const cv::Mat out = FilterProcessor::apply(src, 50, 0, 0);
    ASSERT_EQ(out.type(), CV_8UC3);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x) {
            const cv::Vec3b p = out.at<cv::Vec3b>(y, x);
            EXPECT_GT(p[0], 100);
            EXPECT_GT(p[1], 100);
            EXPECT_GT(p[2], 100);
        }
    }
}

TEST(FilterProcessorTest, SaturationMinus100MakesGrayscale)
{
    const cv::Mat src(32, 32, CV_8UC3, cv::Scalar(200, 50, 30));  // BGR

    const cv::Mat out = FilterProcessor::apply(src, 0, 0, -100);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x) {
            const cv::Vec3b p = out.at<cv::Vec3b>(y, x);
            EXPECT_EQ(p[0], p[1]);
            EXPECT_EQ(p[1], p[2]);
        }
    }
}

TEST(FilterProcessorTest, ContrastMinus100CollapsesToBlack)
{
    const cv::Mat src(32, 32, CV_8UC3, cv::Scalar(100, 120, 140));

    const cv::Mat out = FilterProcessor::apply(src, 0, -100, 0);
    EXPECT_EQ(cv::countNonZero(out.reshape(1)), 0);
}

TEST(FilterProcessorTest, SourceIsNotMutated)
{
    const cv::Mat src(32, 32, CV_8UC3, cv::Scalar(100, 100, 100));
    const cv::Mat before = src.clone();

    FilterProcessor::apply(src, 50, 20, 30);
    cv::Mat diff;
    cv::absdiff(src, before, diff);
    EXPECT_EQ(cv::countNonZero(diff.reshape(1)), 0);
}

TEST(FilterProcessorTest, AlphaChannelIsPreserved)
{
    const cv::Mat src(32, 32, CV_8UC4, cv::Scalar(100, 100, 100, 77));  // BGRA

    const cv::Mat out = FilterProcessor::apply(src, 50, 0, 0);
    ASSERT_EQ(out.type(), CV_8UC4);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x) {
            const cv::Vec4b p = out.at<cv::Vec4b>(y, x);
            EXPECT_EQ(p[3], 77);      // alpha untouched
            EXPECT_GT(p[0], 100);     // brightness applied to color channels
        }
    }
}

TEST(FilterProcessorTest, BlurKeepsUniformImageUnchanged)
{
    const cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    for (BlurType type : {BlurType::Box, BlurType::Gaussian, BlurType::Median}) {
        BlurParams params;
        params.type = type;
        params.kernelSize = 5;
        const cv::Mat out = applyBlur(gray, params);
        cv::Mat diff;
        cv::absdiff(out, gray, diff);
        EXPECT_EQ(cv::countNonZero(diff), 0);
    }
}

TEST(FilterProcessorTest, BoxBlurSpreadsAnImpulse)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(16, 16) = 255;
    BlurParams params;
    params.type = BlurType::Box;
    params.kernelSize = 3;

    const cv::Mat out = applyBlur(gray, params);
    // (255 + 8*100) / 9 = 117.2 -> rounded 117
    EXPECT_NEAR(int(out.at<uchar>(16, 16)), 117, 2);
    EXPECT_NEAR(int(out.at<uchar>(16, 17)), 117, 2);
    EXPECT_EQ(out.at<uchar>(18, 18), 100);  // outside the 3x3 reach
}

TEST(FilterProcessorTest, GaussianBlurSmoothsAnImpulse)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(16, 16) = 255;
    BlurParams params;
    params.type = BlurType::Gaussian;
    params.kernelSize = 5;
    params.sigmaX = 1.5;
    params.sigmaY = 1.5;

    const cv::Mat out = applyBlur(gray, params);
    EXPECT_LT(out.at<uchar>(16, 16), 255);
    EXPECT_GT(out.at<uchar>(16, 16), 100);
    EXPECT_EQ(out.at<uchar>(0, 0), 100);  // outside the kernel reach
}

TEST(FilterProcessorTest, MedianBlurRemovesIsolatedNoise)
{
    cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(8, 8) = 255;  // salt
    gray.at<uchar>(4, 4) = 0;    // pepper
    BlurParams params;
    params.type = BlurType::Median;
    params.kernelSize = 3;

    const cv::Mat out = applyBlur(gray, params);
    EXPECT_EQ(out.at<uchar>(8, 8), 100);
    EXPECT_EQ(out.at<uchar>(4, 4), 100);
    EXPECT_EQ(out.at<uchar>(0, 0), 100);
}

TEST(FilterProcessorTest, CustomKernelIdentityAndDelta)
{
    const cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));
    BlurParams params;
    params.type = BlurType::Custom;
    // The default kernel is the identity
    cv::Mat out = applyBlur(gray, params);
    cv::Mat diff;
    cv::absdiff(out, gray, diff);
    EXPECT_EQ(cv::countNonZero(diff), 0);

    // Delta offsets every pixel
    params.delta = 10;
    out = applyBlur(gray, params);
    EXPECT_EQ(out.at<uchar>(8, 8), 110);
}

TEST(FilterProcessorTest, BlurKeepsColorTypeAndSize)
{
    const cv::Mat bgra(16, 16, CV_8UC4, cv::Scalar(30, 120, 200, 77));
    BlurParams params;
    params.type = BlurType::Gaussian;
    params.kernelSize = 3;

    const cv::Mat out = applyBlur(bgra, params);
    EXPECT_EQ(out.type(), CV_8UC4);
    EXPECT_EQ(out.size(), bgra.size());
}

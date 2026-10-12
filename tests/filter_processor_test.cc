#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/filter_processor.h"

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

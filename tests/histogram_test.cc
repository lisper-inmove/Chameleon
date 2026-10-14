#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/histogram.h"

using chameleon::computeHistogram;

TEST(HistogramTest, UniformGrayImagePutsAllPixelsInOneBin)
{
    const cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));

    const auto bins = computeHistogram(gray, 0);
    EXPECT_DOUBLE_EQ(bins[100], 32 * 32);
    double total = 0;
    for (double v : bins)
        total += v;
    EXPECT_DOUBLE_EQ(total, 32 * 32);
}

TEST(HistogramTest, ColorImageBinsArePerChannel)
{
    // BGR (10, 20, 30) -> channel 0=B, 1=G, 2=R
    const cv::Mat bgr(16, 16, CV_8UC3, cv::Scalar(10, 20, 30));

    EXPECT_DOUBLE_EQ(computeHistogram(bgr, 0)[10], 16 * 16);
    EXPECT_DOUBLE_EQ(computeHistogram(bgr, 1)[20], 16 * 16);
    EXPECT_DOUBLE_EQ(computeHistogram(bgr, 2)[30], 16 * 16);
}

TEST(HistogramTest, EmptyImageAndBadChannelReturnZeros)
{
    const cv::Mat empty;
    double total = 0;
    for (double v : computeHistogram(empty, 0))
        total += v;
    EXPECT_DOUBLE_EQ(total, 0.0);

    const cv::Mat gray(8, 8, CV_8UC1, cv::Scalar(50));
    total = 0;
    for (double v : computeHistogram(gray, 2))  // channel out of range
        total += v;
    EXPECT_DOUBLE_EQ(total, 0.0);
}

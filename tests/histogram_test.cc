#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/histogram.h"

using chameleon::compareHistograms;
using chameleon::matchHistogram;
using chameleon::normalizedSimilarity;
using chameleon::computeHistogram;
using chameleon::equalizeClahe;
using chameleon::equalizeHistogram;
using chameleon::HistogramCompareMethod;

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

TEST(HistogramTest, EqualizeHistogramLeavesUniformImageUnchanged)
{
    // Degenerate case: a single-level image has nothing to spread, and
    // OpenCV's equalizeHist guards the zero-denominator case by copying
    // the source through unchanged
    const cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));

    const cv::Mat out = equalizeHistogram(gray);
    ASSERT_EQ(out.type(), CV_8UC1);
    cv::Mat diff;
    cv::absdiff(out, gray, diff);
    EXPECT_EQ(cv::countNonZero(diff), 0);
}

TEST(HistogramTest, EqualizeHistogramSpreadsLowContrastImage)
{
    cv::Mat gray(16, 32, CV_8UC1);
    gray.colRange(0, 16).setTo(50);
    gray.colRange(16, 32).setTo(150);
    double mn0 = 0, mx0 = 0;
    cv::minMaxLoc(gray, &mn0, &mx0);

    const cv::Mat out = equalizeHistogram(gray);
    double mn = 0, mx = 0;
    cv::minMaxLoc(out, &mn, &mx);
    EXPECT_GT(mx - mn, mx0 - mn0);
    EXPECT_DOUBLE_EQ(mn, 0.0);
    EXPECT_DOUBLE_EQ(mx, 255.0);
}

TEST(HistogramTest, ClaheSpreadsLocalContrast)
{
    cv::Mat gray(64, 64, CV_8UC1);
    for (int y = 0; y < gray.rows; ++y) {
        for (int x = 0; x < gray.cols; ++x)
            gray.at<uchar>(y, x) = uchar(100 + x * 40 / 63);  // 100..140
    }
    double mn0 = 0, mx0 = 0;
    cv::minMaxLoc(gray, &mn0, &mx0);
    ASSERT_LT(mx0 - mn0, 100.0);  // low contrast input

    const cv::Mat out = equalizeClahe(gray);
    ASSERT_EQ(out.type(), CV_8UC1);
    double mn = 0, mx = 0;
    cv::minMaxLoc(out, &mn, &mx);
    EXPECT_GT(mx - mn, mx0 - mn0);
}

TEST(HistogramTest, EqualizeHistogramKeepsColorTypeAndSize)
{
    const cv::Mat bgr(16, 16, CV_8UC3, cv::Scalar(30, 120, 200));

    const cv::Mat out = equalizeHistogram(bgr);
    EXPECT_EQ(out.type(), CV_8UC3);
    EXPECT_EQ(out.size(), bgr.size());
}

TEST(HistogramTest, EqualizeHistogramPreservesAlpha)
{
    const cv::Mat bgra(16, 16, CV_8UC4, cv::Scalar(30, 120, 200, 77));

    const cv::Mat out = equalizeHistogram(bgra);
    ASSERT_EQ(out.type(), CV_8UC4);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x)
            EXPECT_EQ(out.at<cv::Vec4b>(y, x)[3], 77);
    }
}

TEST(HistogramTest, CompareIdenticalImagesScores)
{
    const cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));

    const auto correl = compareHistograms(gray, gray, HistogramCompareMethod::Correlation);
    ASSERT_EQ(correl.size(), 1u);
    EXPECT_NEAR(correl[0], 1.0, 1e-9);

    const auto chisqr = compareHistograms(gray, gray, HistogramCompareMethod::ChiSquare);
    EXPECT_NEAR(chisqr[0], 0.0, 1e-9);

    // Histograms are L1-normalized before comparing, so identical
    // distributions intersect completely at 1.0
    const auto intersect = compareHistograms(gray, gray, HistogramCompareMethod::Intersect);
    EXPECT_NEAR(intersect[0], 1.0, 1e-6);

    const auto bhatt =
        compareHistograms(gray, gray, HistogramCompareMethod::Bhattacharyya);
    EXPECT_NEAR(bhatt[0], 0.0, 1e-9);
}

TEST(HistogramTest, CompareDisjointImagesScores)
{
    const cv::Mat dark(16, 16, CV_8UC1, cv::Scalar(100));
    const cv::Mat bright(16, 16, CV_8UC1, cv::Scalar(200));

    const auto intersect =
        compareHistograms(dark, bright, HistogramCompareMethod::Intersect);
    EXPECT_NEAR(intersect[0], 0.0, 1e-6);

    const auto bhatt =
        compareHistograms(dark, bright, HistogramCompareMethod::Bhattacharyya);
    EXPECT_NEAR(bhatt[0], 1.0, 1e-6);

    const auto correl =
        compareHistograms(dark, bright, HistogramCompareMethod::Correlation);
    EXPECT_LT(correl[0], 0.5);
}

TEST(HistogramTest, CompareIsSizeInvariantAfterNormalization)
{
    // The same distribution at different image sizes must compare as
    // identical (raw compareHist would scale with the pixel count)
    const cv::Mat small(16, 16, CV_8UC1, cv::Scalar(100));
    const cv::Mat large(32, 32, CV_8UC1, cv::Scalar(100));

    const auto intersect =
        compareHistograms(small, large, HistogramCompareMethod::Intersect);
    EXPECT_NEAR(intersect[0], 1.0, 1e-6);

    const auto bhatt =
        compareHistograms(small, large, HistogramCompareMethod::Bhattacharyya);
    EXPECT_NEAR(bhatt[0], 0.0, 1e-6);
}

TEST(HistogramTest, NormalizedSimilarityMapsEveryMethodToOne)
{
    // All methods map identical distributions to 1.0
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Correlation, 1.0), 1.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::ChiSquare, 0.0), 1.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Intersect, 1.0), 1.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Bhattacharyya, 0.0), 1.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::KlDivergence, 0.0), 1.0, 1e-9);
}

TEST(HistogramTest, NormalizedSimilarityMappingsAndClamping)
{
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Correlation, -1.0), 0.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Correlation, 0.0), 0.5, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::ChiSquare, 1.0), 0.5, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::KlDivergence, 3.0), 0.25, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Bhattacharyya, 1.0), 0.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Bhattacharyya, 1.5), 0.0, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Intersect, 0.5), 0.5, 1e-9);
    EXPECT_NEAR(normalizedSimilarity(HistogramCompareMethod::Intersect, 1.5), 1.0, 1e-9);
}

TEST(HistogramTest, CompareUsesMinChannelCount)
{
    const cv::Mat color(16, 16, CV_8UC3, cv::Scalar(10, 20, 30));
    const cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));

    const auto scores = compareHistograms(color, gray, HistogramCompareMethod::Correlation);
    EXPECT_EQ(scores.size(), 1u);

    const auto colorScores =
        compareHistograms(color, color, HistogramCompareMethod::Correlation);
    EXPECT_EQ(colorScores.size(), 3u);
}

TEST(HistogramTest, MatchHistogramToSelfIsIdentity)
{
    cv::Mat twoLevel(16, 16, CV_8UC1);
    twoLevel.colRange(0, 8).setTo(50);
    twoLevel.colRange(8, 16).setTo(150);

    const cv::Mat out = matchHistogram(twoLevel, twoLevel);
    ASSERT_EQ(out.type(), CV_8UC1);
    cv::Mat diff;
    cv::absdiff(out, twoLevel, diff);
    EXPECT_EQ(cv::countNonZero(diff), 0);
}

TEST(HistogramTest, MatchProducesTemplateDistribution)
{
    // Source: a low-contrast horizontal gradient, 100..140
    cv::Mat gradient(16, 16, CV_8UC1);
    for (int y = 0; y < gradient.rows; ++y) {
        for (int x = 0; x < gradient.cols; ++x)
            gradient.at<uchar>(y, x) = uchar(100 + x * 40 / 15);
    }
    // Template: half the pixels at 50, half at 150
    cv::Mat templateImage(16, 16, CV_8UC1);
    templateImage.colRange(0, 8).setTo(50);
    templateImage.colRange(8, 16).setTo(150);

    const cv::Mat out = matchHistogram(gradient, templateImage);
    // The lower half of the source CDF lands on 50, the upper half on 150
    const auto bins = computeHistogram(out, 0);
    EXPECT_DOUBLE_EQ(bins[50], 128.0);
    EXPECT_DOUBLE_EQ(bins[150], 128.0);
    double total = 0;
    for (double v : bins)
        total += v;
    EXPECT_DOUBLE_EQ(total, 256.0);
}

TEST(HistogramTest, MatchKeepsColorTypeAndSize)
{
    const cv::Mat source(16, 16, CV_8UC3, cv::Scalar(30, 120, 200));
    const cv::Mat templateImage(16, 16, CV_8UC1, cv::Scalar(100));

    const cv::Mat out = matchHistogram(source, templateImage);
    EXPECT_EQ(out.type(), CV_8UC3);
    EXPECT_EQ(out.size(), source.size());
}

TEST(HistogramTest, MatchPreservesAlpha)
{
    const cv::Mat source(16, 16, CV_8UC4, cv::Scalar(30, 120, 200, 77));
    const cv::Mat templateImage(16, 16, CV_8UC1, cv::Scalar(100));

    const cv::Mat out = matchHistogram(source, templateImage);
    ASSERT_EQ(out.type(), CV_8UC4);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x)
            EXPECT_EQ(out.at<cv::Vec4b>(y, x)[3], 77);
    }
}

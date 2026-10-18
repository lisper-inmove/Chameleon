#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/binary.h"

using chameleon::AdaptiveThresholdMethod;
using chameleon::colorizeLabels;
using chameleon::connectedComponents;
using chameleon::ConnectedComponentsParams;
using chameleon::applyThreshold;
using chameleon::ThresholdMethod;
using chameleon::ThresholdParams;
using chameleon::ThresholdType;

namespace {

cv::Mat twoLevelImage()
{
    cv::Mat gray(16, 32, CV_8UC1, cv::Scalar(0));
    gray.colRange(0, 16).setTo(100);
    gray.colRange(16, 32).setTo(200);
    return gray;
}

} // namespace

TEST(BinaryTest, BinarySplitsAtTheThreshold)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;
    params.thresh = 150;
    params.maxval = 255;
    params.type = ThresholdType::Binary;

    const cv::Mat out = applyThreshold(gray, params);
    ASSERT_EQ(out.type(), CV_8UC1);
    EXPECT_EQ(out.at<uchar>(0, 8), 0);     // 100 < 150
    EXPECT_EQ(out.at<uchar>(0, 24), 255);  // 200 > 150
}

TEST(BinaryTest, BinaryInvInvertsTheResult)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;
    params.thresh = 150;
    params.maxval = 255;
    params.type = ThresholdType::BinaryInv;

    const cv::Mat out = applyThreshold(gray, params);
    EXPECT_EQ(out.at<uchar>(0, 8), 255);
    EXPECT_EQ(out.at<uchar>(0, 24), 0);
}

TEST(BinaryTest, MaxvalIsRespected)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;
    params.thresh = 150;
    params.maxval = 100;
    params.type = ThresholdType::Binary;

    EXPECT_EQ(applyThreshold(gray, params).at<uchar>(0, 24), 100);
}

TEST(BinaryTest, TruncCapsValuesAtTheThreshold)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;
    params.thresh = 150;
    params.maxval = 255;
    params.type = ThresholdType::Trunc;

    const cv::Mat out = applyThreshold(gray, params);
    EXPECT_EQ(out.at<uchar>(0, 8), 100);   // below: unchanged
    EXPECT_EQ(out.at<uchar>(0, 24), 150);  // above: capped at thresh
}

TEST(BinaryTest, ToZeroVariantsKeepOneSide)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;
    params.thresh = 150;
    params.maxval = 255;

    params.type = ThresholdType::ToZero;
    const cv::Mat toZero = applyThreshold(gray, params);
    EXPECT_EQ(toZero.at<uchar>(0, 8), 0);
    EXPECT_EQ(toZero.at<uchar>(0, 24), 200);

    params.type = ThresholdType::ToZeroInv;
    const cv::Mat toZeroInv = applyThreshold(gray, params);
    EXPECT_EQ(toZeroInv.at<uchar>(0, 8), 100);
    EXPECT_EQ(toZeroInv.at<uchar>(0, 24), 0);
}

TEST(BinaryTest, ThresholdPreservesAlphaAndType)
{
    const cv::Mat bgra(16, 16, CV_8UC4, cv::Scalar(30, 120, 200, 77));
    ThresholdParams params;
    params.thresh = 150;
    params.maxval = 255;
    params.type = ThresholdType::Binary;

    const cv::Mat out = applyThreshold(bgra, params);
    ASSERT_EQ(out.type(), CV_8UC4);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x)
            EXPECT_EQ(out.at<cv::Vec4b>(y, x)[3], 77);  // alpha preserved
    }

    const cv::Mat bgr(16, 16, CV_8UC3, cv::Scalar(30, 120, 200));
    const cv::Mat colorOut = applyThreshold(bgr, params);
    EXPECT_EQ(colorOut.type(), CV_8UC3);
    EXPECT_EQ(colorOut.size(), bgr.size());
}

TEST(BinaryTest, EmptyInputReturnsEmpty)
{
    ThresholdParams params;
    EXPECT_TRUE(applyThreshold(cv::Mat(), params).empty());
}

TEST(BinaryTest, OtsuComputesItsThresholdAndBinarizes)
{
    const cv::Mat gray = twoLevelImage();  // 100 | 200, equal halves
    ThresholdParams params;
    params.method = ThresholdMethod::Otsu;
    params.maxval = 255;
    params.type = ThresholdType::Binary;

    double computed = 0;
    const cv::Mat out = applyThreshold(gray, params, &computed);
    // The optimum lies between the two levels; both classes survive
    EXPECT_GE(computed, 100.0);
    EXPECT_LT(computed, 200.0);
    EXPECT_EQ(out.at<uchar>(0, 8), 0);
    EXPECT_EQ(out.at<uchar>(0, 24), 255);
}

TEST(BinaryTest, TriangleComputesItsThresholdAndBinarizes)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;
    params.method = ThresholdMethod::Triangle;
    params.maxval = 255;
    params.type = ThresholdType::Binary;

    double computed = 0;
    const cv::Mat out = applyThreshold(gray, params, &computed);
    EXPECT_GT(computed, 0.0);
    EXPECT_EQ(out.at<uchar>(0, 8), 0);
    EXPECT_EQ(out.at<uchar>(0, 24), 255);

    // Consistency: feeding the computed value back as a fixed threshold
    // gives the identical result
    ThresholdParams fixed;
    fixed.thresh = computed;
    fixed.maxval = 255;
    fixed.type = ThresholdType::Binary;
    cv::Mat same;
    cv::absdiff(out, applyThreshold(gray, fixed), same);
    EXPECT_EQ(cv::countNonZero(same), 0);
}

TEST(BinaryTest, AdaptiveThresholdIsLocalAndBinary)
{
    const cv::Mat uniform(32, 32, CV_8UC1, cv::Scalar(100));
    ThresholdParams params;
    params.method = ThresholdMethod::Adaptive;
    params.maxval = 255;
    params.type = ThresholdType::Binary;
    params.adaptiveMethod = AdaptiveThresholdMethod::Mean;
    params.blockSize = 11;
    params.c = 5;

    // 100 > local mean (100) - 5 everywhere
    const cv::Mat out = applyThreshold(uniform, params);
    double mn = 0, mx = 0;
    cv::minMaxLoc(out, &mn, &mx);
    EXPECT_DOUBLE_EQ(mn, 255.0);
    EXPECT_DOUBLE_EQ(mx, 255.0);

    params.type = ThresholdType::BinaryInv;
    cv::minMaxLoc(applyThreshold(uniform, params), &mn, &mx);
    EXPECT_DOUBLE_EQ(mx, 0.0);
}

TEST(BinaryTest, AdaptiveConstantAndGaussianVariantArePlumbed)
{
    const cv::Mat uniform(32, 32, CV_8UC1, cv::Scalar(100));
    ThresholdParams params;
    params.method = ThresholdMethod::Adaptive;
    params.maxval = 255;
    params.type = ThresholdType::Binary;
    params.blockSize = 11;

    // A negative constant pushes the uniform image below the local mean
    params.c = -10;
    double mn = 0, mx = 0;
    cv::minMaxLoc(applyThreshold(uniform, params), &mn, &mx);
    EXPECT_DOUBLE_EQ(mx, 0.0);

    params.adaptiveMethod = AdaptiveThresholdMethod::Gaussian;
    params.c = 5;
    cv::minMaxLoc(applyThreshold(uniform, params), &mn, &mx);
    EXPECT_DOUBLE_EQ(mn, 255.0);
}

TEST(BinaryTest, AutoThresholdOnColorKeepsChannelsAndAlpha)
{
    cv::Mat color(16, 32, CV_8UC3, cv::Scalar(100, 100, 100));
    color.colRange(16, 32).setTo(cv::Scalar(200, 200, 200));
    ThresholdParams params;
    params.method = ThresholdMethod::Otsu;
    params.maxval = 255;

    const cv::Mat out = applyThreshold(color, params);
    EXPECT_EQ(out.type(), CV_8UC3);
    EXPECT_EQ(out.size(), color.size());
    const cv::Vec3b low = out.at<cv::Vec3b>(0, 8);
    const cv::Vec3b high = out.at<cv::Vec3b>(0, 24);
    EXPECT_EQ(int(low[0]) + int(low[1]) + int(low[2]), 0);
    EXPECT_EQ(int(high[0]) + int(high[1]) + int(high[2]), 765);

    cv::Mat bgra(16, 32, CV_8UC4, cv::Scalar(100, 100, 100, 77));
    bgra.colRange(16, 32).setTo(cv::Scalar(200, 200, 200, 77));
    const cv::Mat outAlpha = applyThreshold(bgra, params);
    ASSERT_EQ(outAlpha.type(), CV_8UC4);
    for (int y = 0; y < outAlpha.rows; ++y) {
        for (int x = 0; x < outAlpha.cols; ++x)
            EXPECT_EQ(outAlpha.at<cv::Vec4b>(y, x)[3], 77);
    }
}

TEST(BinaryTest, FixedMethodReportsTheGivenThreshold)
{
    const cv::Mat gray = twoLevelImage();
    ThresholdParams params;  // Fixed by default
    params.thresh = 150;
    double computed = 0;
    applyThreshold(gray, params, &computed);
    EXPECT_DOUBLE_EQ(computed, 150.0);
}

namespace {

// Black 64x64 canvas with three well-separated white rectangles
cv::Mat threeRectsImage()
{
    cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(0));
    gray(cv::Rect(5, 5, 10, 10)).setTo(255);    // A: area 100
    gray(cv::Rect(30, 30, 15, 15)).setTo(255);  // B: area 225
    gray(cv::Rect(50, 5, 8, 8)).setTo(255);     // C: area 64
    return gray;
}

} // namespace

TEST(BinaryTest, ConnectedComponentsCountsRegions)
{
    const cv::Mat gray = threeRectsImage();
    ConnectedComponentsParams params;
    params.connectivity = 8;
    params.withStats = false;

    const auto result = connectedComponents(gray, params);
    ASSERT_EQ(result.labels.type(), CV_32S);
    EXPECT_EQ(result.count, 4);  // background + 3 rectangles
    EXPECT_TRUE(result.areas.empty());  // no stats requested

    const int labelA = result.labels.at<int>(7, 7);
    const int labelB = result.labels.at<int>(35, 35);
    const int labelC = result.labels.at<int>(7, 52);  // (row, col)
    EXPECT_NE(labelA, 0);  // not background
    EXPECT_NE(labelA, labelB);
    EXPECT_NE(labelB, labelC);
    EXPECT_EQ(labelA, result.labels.at<int>(13, 13));  // same rectangle
    EXPECT_EQ(result.labels.at<int>(0, 0), 0);         // background
}

TEST(BinaryTest, ConnectivitySeparatesDiagonalRegions)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(0));
    gray(cv::Rect(5, 5, 8, 8)).setTo(255);
    gray(cv::Rect(13, 13, 8, 8)).setTo(255);  // touches the first diagonally

    ConnectedComponentsParams params;
    params.withStats = false;

    params.connectivity = 8;
    EXPECT_EQ(connectedComponents(gray, params).count, 2);  // merged

    params.connectivity = 4;
    EXPECT_EQ(connectedComponents(gray, params).count, 3);  // separate
}

TEST(BinaryTest, WithStatsReportsAreasBoxesAndCentroids)
{
    const cv::Mat gray = threeRectsImage();
    ConnectedComponentsParams params;
    params.connectivity = 8;
    params.withStats = true;

    const auto result = connectedComponents(gray, params);
    ASSERT_EQ(result.count, 4);
    ASSERT_EQ(result.areas.size(), 4u);
    ASSERT_EQ(result.boundingBoxes.size(), 4u);

    const int labelA = result.labels.at<int>(7, 7);
    const int labelB = result.labels.at<int>(35, 35);
    const int labelC = result.labels.at<int>(7, 52);  // (row, col)
    EXPECT_EQ(result.areas[labelA], 100);
    EXPECT_EQ(result.areas[labelB], 225);
    EXPECT_EQ(result.areas[labelC], 64);
    EXPECT_EQ(result.boundingBoxes[labelA], cv::Rect(5, 5, 10, 10));
    EXPECT_EQ(result.boundingBoxes[labelB], cv::Rect(30, 30, 15, 15));
    EXPECT_NEAR(result.centroids[labelB].x, 37.0, 0.5);
    EXPECT_NEAR(result.centroids[labelB].y, 37.0, 0.5);
    EXPECT_EQ(result.areas[0], 64 * 64 - (100 + 225 + 64));  // background
}

TEST(BinaryTest, ColorizeLabelsGivesDistinctColorsPerRegion)
{
    const cv::Mat gray = threeRectsImage();
    ConnectedComponentsParams params;
    params.withStats = false;
    const auto result = connectedComponents(gray, params);

    const cv::Mat colored = colorizeLabels(result.labels);
    ASSERT_EQ(colored.type(), CV_8UC3);
    EXPECT_EQ(colored.at<cv::Vec3b>(0, 0), cv::Vec3b(0, 0, 0));  // background

    const cv::Vec3b colorA = colored.at<cv::Vec3b>(7, 7);
    const cv::Vec3b colorB = colored.at<cv::Vec3b>(35, 35);
    const cv::Vec3b colorC = colored.at<cv::Vec3b>(7, 52);
    EXPECT_NE(colorA, cv::Vec3b(0, 0, 0));
    EXPECT_NE(colorA, colorB);
    EXPECT_NE(colorB, colorC);
    EXPECT_EQ(colorA, colored.at<cv::Vec3b>(13, 13));  // same region
}

TEST(BinaryTest, ColorizeLabelsCanAnnotateComponentIds)
{
    const cv::Mat gray = threeRectsImage();
    ConnectedComponentsParams params;
    params.withStats = false;
    const auto result = connectedComponents(gray, params);

    const cv::Mat plain = colorizeLabels(result.labels);
    const cv::Mat annotated = colorizeLabels(result.labels, true);

    // The annotation draws something extra
    cv::Mat diff;
    cv::absdiff(plain, annotated, diff);
    EXPECT_GT(cv::countNonZero(diff.reshape(1)), 0);

    // IDs are drawn as near-white glyph pixels, which the plain map has
    // none of (component colors are saturated)
    auto countWhite = [](const cv::Mat &image) {
        int count = 0;
        for (int y = 0; y < image.rows; ++y) {
            const cv::Vec3b *row = image.ptr<cv::Vec3b>(y);
            for (int x = 0; x < image.cols; ++x) {
                if (row[x][0] > 240 && row[x][1] > 240 && row[x][2] > 240)
                    ++count;
            }
        }
        return count;
    };
    EXPECT_EQ(countWhite(plain), 0);
    EXPECT_GT(countWhite(annotated), 10);

    // The background corner stays black (only components get IDs)
    EXPECT_EQ(annotated.at<cv::Vec3b>(0, 0), cv::Vec3b(0, 0, 0));
}

TEST(BinaryTest, ColorInputIsConvertedToLuminance)
{
    cv::Mat color(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    color(cv::Rect(5, 5, 8, 8)).setTo(cv::Scalar(255, 255, 255));
    ConnectedComponentsParams params;
    params.withStats = false;

    const auto result = connectedComponents(color, params);
    EXPECT_EQ(result.count, 2);
}

TEST(BinaryTest, ConnectedComponentsEmptyInput)
{
    ConnectedComponentsParams params;
    const auto result = connectedComponents(cv::Mat(), params);
    EXPECT_EQ(result.count, 0);
    EXPECT_TRUE(result.labels.empty());
    EXPECT_TRUE(colorizeLabels(cv::Mat()).empty());
}

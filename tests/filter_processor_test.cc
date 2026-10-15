#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/filter_processor.h"

using chameleon::applyBlur;
using chameleon::applyCanny;
using chameleon::applyLaplacianSharpen;
using chameleon::imageSharpness;
using chameleon::SharpenParams;
using chameleon::applyGradient;
using chameleon::CannyParams;
using chameleon::GradientParams;
using chameleon::GradientType;
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

TEST(FilterProcessorTest, DirectionalBlursKeepUniformImageUnchanged)
{
    const cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    for (BlurType type : {BlurType::HorizontalBox, BlurType::VerticalBox,
                          BlurType::DiagonalBox}) {
        BlurParams params;
        params.type = type;
        params.kernelSize = 5;
        const cv::Mat out = applyBlur(gray, params);
        cv::Mat diff;
        cv::absdiff(out, gray, diff);
        EXPECT_EQ(cv::countNonZero(diff), 0);
    }
}

TEST(FilterProcessorTest, HorizontalBlurSpreadsAlongTheRowOnly)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(16, 16) = 255;
    BlurParams params;
    params.type = BlurType::HorizontalBox;
    params.kernelSize = 5;

    const cv::Mat out = applyBlur(gray, params);
    // Along the row: (255 + 4*100) / 5 = 131
    EXPECT_EQ(out.at<uchar>(16, 16), 131);
    EXPECT_EQ(out.at<uchar>(16, 17), 131);
    // No vertical spread
    EXPECT_EQ(out.at<uchar>(15, 16), 100);
    EXPECT_EQ(out.at<uchar>(17, 16), 100);
    // Outside the 5-wide window on the row
    EXPECT_EQ(out.at<uchar>(16, 13), 100);
}

TEST(FilterProcessorTest, VerticalBlurSpreadsAlongTheColumnOnly)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(16, 16) = 255;
    BlurParams params;
    params.type = BlurType::VerticalBox;
    params.kernelSize = 5;

    const cv::Mat out = applyBlur(gray, params);
    EXPECT_EQ(out.at<uchar>(15, 16), 131);
    EXPECT_EQ(out.at<uchar>(17, 16), 131);
    EXPECT_EQ(out.at<uchar>(16, 15), 100);  // no horizontal spread
    EXPECT_EQ(out.at<uchar>(16, 17), 100);
}

TEST(FilterProcessorTest, DiagonalBlurSpreadsAlongTheDiagonalOnly)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(16, 16) = 255;
    BlurParams params;
    params.type = BlurType::DiagonalBox;
    params.kernelSize = 5;

    const cv::Mat out = applyBlur(gray, params);
    EXPECT_EQ(out.at<uchar>(15, 15), 131);
    EXPECT_EQ(out.at<uchar>(17, 17), 131);
    EXPECT_EQ(out.at<uchar>(16, 17), 100);  // off-diagonal untouched
    EXPECT_EQ(out.at<uchar>(17, 16), 100);
}

TEST(FilterProcessorTest, HorizontalBlurRespectsTheKernelSize)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(16, 16) = 255;
    BlurParams params;
    params.type = BlurType::HorizontalBox;
    params.kernelSize = 3;

    const cv::Mat out = applyBlur(gray, params);
    // (255 + 2*100) / 3 = 151.7 -> rounded 152
    EXPECT_EQ(out.at<uchar>(16, 15), 152);
    EXPECT_EQ(out.at<uchar>(16, 14), 100);  // outside the 3-wide window
}

namespace {
cv::Mat verticalEdgeImage()
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(0));
    gray.colRange(16, 32).setTo(255);
    return gray;
}
} // namespace

TEST(FilterProcessorTest, PrewittDetectsVerticalEdges)
{
    const cv::Mat gray = verticalEdgeImage();
    GradientParams params;
    params.type = GradientType::Prewitt;
    params.dx = 1;
    params.dy = 0;

    const cv::Mat out = applyGradient(gray, params);
    ASSERT_EQ(out.type(), CV_8UC1);
    // The step edge produces a two-column response band (15 and 16);
    // beyond it the tap window is uniform again
    EXPECT_EQ(out.at<uchar>(8, 15), 255);
    EXPECT_EQ(out.at<uchar>(8, 16), 255);
    EXPECT_EQ(out.at<uchar>(8, 17), 0);
    EXPECT_EQ(out.at<uchar>(8, 4), 0);
}

TEST(FilterProcessorTest, PrewittHorizontalKernelIgnoresVerticalEdges)
{
    const cv::Mat gray = verticalEdgeImage();
    GradientParams params;
    params.type = GradientType::Prewitt;
    params.dx = 0;
    params.dy = 1;

    const cv::Mat out = applyGradient(gray, params);
    // Rows are constant, so the vertical kernel response is zero everywhere
    EXPECT_EQ(cv::countNonZero(out), 0);
}

TEST(FilterProcessorTest, SobelDetectsVerticalEdges)
{
    const cv::Mat gray = verticalEdgeImage();
    GradientParams params;
    params.type = GradientType::Sobel;
    params.dx = 1;
    params.dy = 0;
    params.ksize = 3;

    const cv::Mat out = applyGradient(gray, params);
    EXPECT_EQ(out.at<uchar>(8, 15), 255);
    EXPECT_EQ(out.at<uchar>(8, 16), 255);
    EXPECT_EQ(out.at<uchar>(8, 17), 0);
    EXPECT_EQ(out.at<uchar>(8, 4), 0);
}

TEST(FilterProcessorTest, SobelScaleAndDeltaAffectTheResult)
{
    const cv::Mat gray = verticalEdgeImage();
    GradientParams params;
    params.type = GradientType::Sobel;
    params.dx = 1;
    params.dy = 0;
    params.ksize = 1;  // raw response at the edge is exactly 255

    params.scale = 0.5;
    params.delta = 0;
    EXPECT_EQ(applyGradient(gray, params).at<uchar>(8, 15), 128);  // 127.5

    params.scale = 1.0;
    params.delta = -200;
    EXPECT_EQ(applyGradient(gray, params).at<uchar>(8, 15), 55);  // 255 - 200
}

TEST(FilterProcessorTest, ScharrDetectsVerticalEdges)
{
    const cv::Mat gray = verticalEdgeImage();
    GradientParams params;
    params.type = GradientType::Scharr;
    params.dx = 1;
    params.dy = 0;

    const cv::Mat out = applyGradient(gray, params);
    // Scharr response 16 * 255 = 4080; scale 1/16 normalizes it exactly
    params.scale = 1.0 / 16.0;
    const cv::Mat scaled = applyGradient(gray, params);
    EXPECT_EQ(out.at<uchar>(8, 15), 255);
    EXPECT_EQ(out.at<uchar>(8, 16), 255);
    EXPECT_EQ(out.at<uchar>(8, 17), 0);
    EXPECT_EQ(scaled.at<uchar>(8, 15), 255);
}

TEST(FilterProcessorTest, ScharrForcesASingleDirection)
{
    const cv::Mat gray = verticalEdgeImage();
    GradientParams params;
    params.type = GradientType::Scharr;
    params.dx = 1;
    params.dy = 1;  // invalid for Scharr: normalized to the x direction

    const cv::Mat out = applyGradient(gray, params);
    EXPECT_EQ(out.at<uchar>(8, 15), 255);  // still the vertical-edge response
}

TEST(FilterProcessorTest, GradientKeepsColorTypeAndSize)
{
    const cv::Mat bgr(16, 16, CV_8UC3, cv::Scalar(30, 120, 200));
    for (GradientType type : {GradientType::Prewitt, GradientType::Sobel,
                              GradientType::Scharr}) {
        GradientParams params;
        params.type = type;
        params.dx = 1;
        params.dy = 0;
        const cv::Mat out = applyGradient(bgr, params);
        EXPECT_EQ(out.type(), CV_8UC3);
        EXPECT_EQ(out.size(), bgr.size());
    }
}

TEST(FilterProcessorTest, CannyFindsStrongEdges)
{
    const cv::Mat gray = verticalEdgeImage();
    CannyParams params;
    params.threshold1 = 50;
    params.threshold2 = 150;
    params.apertureSize = 3;

    const cv::Mat out = applyCanny(gray, params);
    ASSERT_EQ(out.type(), CV_8UC1);
    EXPECT_EQ(out.size(), gray.size());
    // The step edge is detected; the response is binary
    EXPECT_GT(cv::countNonZero(out), 0);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x) {
            const uchar v = out.at<uchar>(y, x);
            EXPECT_TRUE(v == 0 || v == 255);
        }
    }
    // Far from the edge nothing is detected
    EXPECT_EQ(out.at<uchar>(8, 4), 0);
    EXPECT_EQ(out.at<uchar>(28, 27), 0);
}

TEST(FilterProcessorTest, CannyThresholdsControlSensitivity)
{
    // Smooth ramp: the gradient magnitude stays around 32
    cv::Mat ramp(16, 32, CV_8UC1);
    for (int y = 0; y < ramp.rows; ++y) {
        for (int x = 0; x < ramp.cols; ++x)
            ramp.at<uchar>(y, x) = uchar(x * 8);
    }

    CannyParams params;
    params.apertureSize = 3;
    params.threshold1 = 200;  // above the ramp gradient
    params.threshold2 = 400;
    EXPECT_EQ(cv::countNonZero(applyCanny(ramp, params)), 0);

    params.threshold1 = 10;
    params.threshold2 = 20;
    EXPECT_GT(cv::countNonZero(applyCanny(ramp, params)), 0);
}

TEST(FilterProcessorTest, CannyApertureAndL2GradientArePlumbed)
{
    const cv::Mat gray = verticalEdgeImage();
    CannyParams params;
    params.threshold1 = 50;
    params.threshold2 = 150;
    params.apertureSize = 5;
    params.l2Gradient = true;

    const cv::Mat out = applyCanny(gray, params);
    EXPECT_EQ(out.type(), CV_8UC1);
    EXPECT_GT(cv::countNonZero(out), 0);
}

TEST(FilterProcessorTest, CannyColorInputKeepsChannels)
{
    cv::Mat color(16, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    color.colRange(16, 32).setTo(cv::Scalar(255, 255, 255));
    CannyParams params;
    params.threshold1 = 50;
    params.threshold2 = 150;

    const cv::Mat out = applyCanny(color, params);
    EXPECT_EQ(out.type(), CV_8UC3);
    EXPECT_EQ(out.size(), color.size());
    EXPECT_GT(cv::countNonZero(out.reshape(1)), 0);

    cv::Mat bgra(16, 32, CV_8UC4, cv::Scalar(0, 0, 0, 77));
    bgra.colRange(16, 32).setTo(cv::Scalar(255, 255, 255, 77));
    const cv::Mat outAlpha = applyCanny(bgra, params);
    ASSERT_EQ(outAlpha.type(), CV_8UC4);
    for (int y = 0; y < outAlpha.rows; ++y) {
        for (int x = 0; x < outAlpha.cols; ++x)
            EXPECT_EQ(outAlpha.at<cv::Vec4b>(y, x)[3], 77);  // alpha preserved
    }
}

TEST(FilterProcessorTest, BilateralKeepsUniformImageUnchanged)
{
    const cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    for (int border : {cv::BORDER_DEFAULT, cv::BORDER_REFLECT, cv::BORDER_REPLICATE}) {
        BlurParams params;
        params.type = BlurType::Bilateral;
        params.kernelSize = 5;
        params.sigmaColor = 50;
        params.sigmaSpace = 50;
        params.borderType = border;

        const cv::Mat out = applyBlur(gray, params);
        cv::Mat diff;
        cv::absdiff(out, gray, diff);
        EXPECT_EQ(cv::countNonZero(diff), 0);
    }
}

TEST(FilterProcessorTest, BilateralPreservesEdgesWithSmallSigmaColor)
{
    const cv::Mat gray = verticalEdgeImage();  // 0 | 255 step
    BlurParams params;
    params.type = BlurType::Bilateral;
    params.kernelSize = 5;
    params.sigmaColor = 10;  // the 255 step is far beyond the color sigma
    params.sigmaSpace = 50;

    const cv::Mat out = applyBlur(gray, params);
    // The edge stays perfectly sharp: no cross-edge blending
    EXPECT_EQ(out.at<uchar>(8, 15), 0);
    EXPECT_EQ(out.at<uchar>(8, 16), 255);
    EXPECT_EQ(out.at<uchar>(8, 8), 0);
}

TEST(FilterProcessorTest, BilateralSmoothsOutliersDependingOnSigmaColor)
{
    cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));
    gray.at<uchar>(8, 8) = 150;  // moderate outlier inside a flat region
    BlurParams params;
    params.type = BlurType::Bilateral;
    params.kernelSize = 5;
    params.sigmaSpace = 50;

    params.sigmaColor = 1;  // the 50-level difference is preserved
    EXPECT_EQ(applyBlur(gray, params).at<uchar>(8, 8), 150);

    params.sigmaColor = 100;  // pulled toward the surrounding 100s
    EXPECT_LT(applyBlur(gray, params).at<uchar>(8, 8), 130);
}

TEST(FilterProcessorTest, LaplacianSharpenKeepsFlatRegionsUnchanged)
{
    const cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    for (int ksize : {1, 3}) {
        SharpenParams params;
        params.ksize = ksize;
        params.strength = 2.0;
        const cv::Mat out = applyLaplacianSharpen(gray, params);
        cv::Mat diff;
        cv::absdiff(out, gray, diff);
        EXPECT_EQ(cv::countNonZero(diff), 0);
    }
}

TEST(FilterProcessorTest, LaplacianSharpenExaggeratesEdges)
{
    cv::Mat gray(16, 32, CV_8UC1, cv::Scalar(100));
    gray.colRange(16, 32).setTo(200);
    SharpenParams params;
    params.ksize = 1;  // the 4-neighbor kernel gives exact arithmetic

    // out = image - strength * laplacian; at the dark side of the edge
    // the laplacian is +100 -> 100 - 100 = 0; at the bright side it is
    // -100 -> 200 + 100 = 300 -> saturated 255
    params.strength = 1.0;
    const cv::Mat out = applyLaplacianSharpen(gray, params);
    EXPECT_EQ(out.at<uchar>(8, 14), 100);  // away from the edge: unchanged
    EXPECT_EQ(out.at<uchar>(8, 15), 0);
    EXPECT_EQ(out.at<uchar>(8, 16), 255);
    EXPECT_EQ(out.at<uchar>(8, 17), 200);

    params.strength = 0.5;
    const cv::Mat half = applyLaplacianSharpen(gray, params);
    EXPECT_EQ(half.at<uchar>(8, 15), 50);
    EXPECT_EQ(half.at<uchar>(8, 16), 250);
}

TEST(FilterProcessorTest, SharpenIncreasesTheSharpnessMetric)
{
    cv::Mat gray(32, 32, CV_8UC1, cv::Scalar(100));
    gray.colRange(16, 32).setTo(200);
    BlurParams blur;
    blur.type = BlurType::Gaussian;
    blur.kernelSize = 7;
    const cv::Mat blurred = applyBlur(gray, blur);

    SharpenParams sharpen;
    sharpen.ksize = 3;
    sharpen.strength = 1.0;
    const cv::Mat sharpened = applyLaplacianSharpen(blurred, sharpen);

    EXPECT_GT(imageSharpness(sharpened), imageSharpness(blurred));
}

TEST(FilterProcessorTest, SharpnessIsZeroOnFlatImagesAndPositiveOnEdges)
{
    const cv::Mat flat(32, 32, CV_8UC1, cv::Scalar(100));
    EXPECT_DOUBLE_EQ(imageSharpness(flat), 0.0);

    cv::Mat edge(32, 32, CV_8UC1, cv::Scalar(100));
    edge.colRange(16, 32).setTo(200);
    EXPECT_GT(imageSharpness(edge), 0.0);

    EXPECT_DOUBLE_EQ(imageSharpness(cv::Mat()), 0.0);
}

TEST(FilterProcessorTest, LaplacianSharpenPreservesAlphaAndType)
{
    const cv::Mat bgra(16, 16, CV_8UC4, cv::Scalar(30, 120, 200, 77));
    SharpenParams params;
    params.ksize = 3;
    params.strength = 1.0;

    const cv::Mat out = applyLaplacianSharpen(bgra, params);
    ASSERT_EQ(out.type(), CV_8UC4);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x)
            EXPECT_EQ(out.at<cv::Vec4b>(y, x)[3], 77);
    }
}

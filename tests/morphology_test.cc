#include <gtest/gtest.h>

#include <opencv2/imgproc.hpp>

#include "processing/morphology.h"

using chameleon::applyMorphology;
using chameleon::MorphologyMethod;
using chameleon::MorphologyOperation;
using chameleon::MorphologyParams;
using chameleon::MorphologyShape;

namespace {

cv::Mat squareImage()
{
    cv::Mat image(60, 60, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Rect(20, 20, 20, 20), cv::Scalar(255),
                  cv::FILLED);
    return image;
}

cv::Mat singlePixelImage()
{
    cv::Mat image(21, 21, CV_8UC1, cv::Scalar(0));
    image.at<uchar>(10, 10) = 255;
    return image;
}

int whiteCount(const cv::Mat &image)
{
    return cv::countNonZero(image);
}

} // namespace

TEST(MorphologyTest, DilateExpandsBrightRegions)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;  // dilate, rect 3x3, centred, one iteration
    const cv::Mat result = applyMorphology(source, params);

    EXPECT_GT(whiteCount(result), whiteCount(source));
    EXPECT_EQ(result.at<uchar>(19, 30), 255);  // one pixel above the square
    EXPECT_EQ(result.at<uchar>(30, 30), 255);  // interior unchanged
}

TEST(MorphologyTest, ErodeShrinksBrightRegions)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;
    params.operation = MorphologyOperation::Erode;
    const cv::Mat result = applyMorphology(source, params);

    EXPECT_LT(whiteCount(result), whiteCount(source));
    EXPECT_EQ(result.at<uchar>(20, 30), 0);    // boundary eroded
    EXPECT_EQ(result.at<uchar>(30, 30), 255);  // centre survives
}

TEST(MorphologyTest, IterationsAmplifyTheEffect)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;
    params.iterations = 1;
    const cv::Mat once = applyMorphology(source, params);
    params.iterations = 2;
    const cv::Mat twice = applyMorphology(source, params);

    EXPECT_GT(whiteCount(twice), whiteCount(once));
}

TEST(MorphologyTest, KernelShapeAndSizeAreApplied)
{
    const cv::Mat single = singlePixelImage();

    MorphologyParams params;  // 3x3 rectangle: 9 pixels
    EXPECT_EQ(whiteCount(applyMorphology(single, params)), 9);

    params.shape = MorphologyShape::Cross;  // 5 pixels
    EXPECT_EQ(whiteCount(applyMorphology(single, params)), 5);

    params.shape = MorphologyShape::Ellipse;  // 3x3 ellipse is a plus shape
    EXPECT_EQ(whiteCount(applyMorphology(single, params)), 5);

    params.shape = MorphologyShape::Rect;
    params.kernelWidth = 5;
    params.kernelHeight = 5;
    EXPECT_EQ(whiteCount(applyMorphology(single, params)), 25);
}

TEST(MorphologyTest, AnchorShiftsTheKernel)
{
    const cv::Mat single = singlePixelImage();
    MorphologyParams params;
    params.anchor = cv::Point(0, 0);  // kernel anchor at the top-left
    const cv::Mat result = applyMorphology(single, params);

    // The anchor is where the kernel samples from: (0,0) grows up/left
    EXPECT_EQ(result.at<uchar>(9, 10), 255);
    EXPECT_EQ(result.at<uchar>(8, 8), 255);
    EXPECT_EQ(result.at<uchar>(10, 10), 255);
    EXPECT_EQ(result.at<uchar>(12, 12), 0);  // no growth right/down
}

TEST(MorphologyTest, BorderTypeAndValueAreApplied)
{
    const cv::Mat white(20, 20, CV_8UC1, cv::Scalar(255));
    MorphologyParams params;
    params.operation = MorphologyOperation::Erode;
    params.borderType = cv::BORDER_CONSTANT;
    params.borderValue = 0;
    cv::Mat result = applyMorphology(white, params);
    EXPECT_EQ(result.at<uchar>(0, 0), 0);      // outside treated as black
    EXPECT_EQ(result.at<uchar>(10, 10), 255);

    params.borderType = cv::BORDER_REPLICATE;
    result = applyMorphology(white, params);
    EXPECT_EQ(result.at<uchar>(0, 0), 255);  // replicated border stays white
}

TEST(MorphologyTest, AlphaChannelIsPreserved)
{
    cv::Mat image(40, 40, CV_8UC4, cv::Scalar(0, 0, 0, 77));
    cv::rectangle(image, cv::Rect(10, 10, 10, 10),
                  cv::Scalar(255, 255, 255, 77), cv::FILLED);

    MorphologyParams params;
    const cv::Mat result = applyMorphology(image, params);

    ASSERT_EQ(result.type(), CV_8UC4);
    EXPECT_EQ(result.at<cv::Vec4b>(20, 20)[3], 77);
    // The newly covered pixel keeps the source alpha
    EXPECT_EQ(result.at<cv::Vec4b>(9, 9)[3], 77);
    EXPECT_EQ(result.at<cv::Vec4b>(9, 9)[0], 255);
}

TEST(MorphologyTest, OpenRemovesSmallBrightDetails)
{
    cv::Mat image(60, 60, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Rect(10, 10, 30, 30), cv::Scalar(255),
                  cv::FILLED);
    image.at<uchar>(50, 50) = 255;  // isolated speck

    MorphologyParams params;
    params.operation = MorphologyOperation::Open;
    const cv::Mat result = applyMorphology(image, params);

    EXPECT_EQ(result.at<uchar>(50, 50), 0);    // speck removed
    EXPECT_EQ(result.at<uchar>(25, 25), 255);  // large square survives
    EXPECT_EQ(result.at<uchar>(10, 10), 255);  // its boundary is restored
}

TEST(MorphologyTest, CloseFillsSmallDarkHoles)
{
    cv::Mat image(60, 60, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Rect(10, 10, 40, 40), cv::Scalar(255),
                  cv::FILLED);
    image.at<uchar>(30, 30) = 0;  // pinhole

    MorphologyParams params;
    params.operation = MorphologyOperation::Close;
    const cv::Mat result = applyMorphology(image, params);

    EXPECT_EQ(result.at<uchar>(30, 30), 255);  // hole filled
    EXPECT_EQ(result.at<uchar>(5, 5), 0);      // background stays black
}

TEST(MorphologyTest, MorphologyExAndCombinedImplementationAgree)
{
    cv::Mat image(64, 64, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Rect(8, 8, 24, 24), cv::Scalar(255), cv::FILLED);
    cv::rectangle(image, cv::Rect(40, 35, 16, 20), cv::Scalar(255),
                  cv::FILLED);
    image.at<uchar>(50, 10) = 255;
    image.at<uchar>(10, 50) = 255;

    for (const auto operation :
         {MorphologyOperation::Open, MorphologyOperation::Close}) {
        MorphologyParams viaEx;
        viaEx.operation = operation;
        viaEx.method = MorphologyMethod::MorphologyEx;
        MorphologyParams combined;
        combined.operation = operation;
        combined.method = MorphologyMethod::Combined;

        const cv::Mat a = applyMorphology(image, viaEx);
        const cv::Mat b = applyMorphology(image, combined);
        ASSERT_EQ(a.size(), b.size());
        EXPECT_EQ(cv::norm(a, b, cv::NORM_INF), 0.0)
            << (operation == MorphologyOperation::Open ? "open" : "close");
    }
}

TEST(MorphologyTest, BasicGradientIsDilationMinusErosion)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;
    params.operation = MorphologyOperation::BasicGradient;
    const cv::Mat result = applyMorphology(source, params);

    EXPECT_EQ(result.at<uchar>(30, 30), 0);    // flat interior
    EXPECT_EQ(result.at<uchar>(20, 30), 255);  // object boundary
    EXPECT_EQ(result.at<uchar>(19, 30), 255);  // just outside
    EXPECT_EQ(result.at<uchar>(5, 5), 0);      // far background
}

TEST(MorphologyTest, InternalGradientStaysInsideTheObject)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;
    params.operation = MorphologyOperation::InternalGradient;
    const cv::Mat result = applyMorphology(source, params);

    EXPECT_EQ(result.at<uchar>(20, 30), 255);  // original - eroded
    EXPECT_EQ(result.at<uchar>(19, 30), 0);    // outside untouched
    EXPECT_EQ(result.at<uchar>(30, 30), 0);    // interior
}

TEST(MorphologyTest, ExternalGradientStaysOutsideTheObject)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;
    params.operation = MorphologyOperation::ExternalGradient;
    const cv::Mat result = applyMorphology(source, params);

    EXPECT_EQ(result.at<uchar>(19, 30), 255);  // dilated - original
    EXPECT_EQ(result.at<uchar>(20, 30), 0);    // boundary unchanged
    EXPECT_EQ(result.at<uchar>(5, 5), 0);
}

TEST(MorphologyTest, GradientWidthFollowsTheKernelSize)
{
    const cv::Mat source = squareImage();
    MorphologyParams params;
    params.operation = MorphologyOperation::BasicGradient;
    params.kernelWidth = 3;
    params.kernelHeight = 3;
    const int thin = whiteCount(applyMorphology(source, params));
    params.kernelWidth = 7;
    params.kernelHeight = 7;
    const int wide = whiteCount(applyMorphology(source, params));

    EXPECT_GT(wide, thin);
}

TEST(MorphologyTest, GradientPreservesAlpha)
{
    cv::Mat image(40, 40, CV_8UC4, cv::Scalar(0, 0, 0, 77));
    cv::rectangle(image, cv::Rect(10, 10, 20, 20),
                  cv::Scalar(255, 255, 255, 77), cv::FILLED);

    MorphologyParams params;
    params.operation = MorphologyOperation::BasicGradient;
    const cv::Mat result = applyMorphology(image, params);

    ASSERT_EQ(result.type(), CV_8UC4);
    EXPECT_EQ(result.at<cv::Vec4b>(0, 0)[3], 77);
    EXPECT_EQ(result.at<cv::Vec4b>(20, 20)[3], 77);
}

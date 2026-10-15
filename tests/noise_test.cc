#include <gtest/gtest.h>

#include <opencv2/core.hpp>

#include "processing/filter_processor.h"
#include "processing/noise.h"

using chameleon::addGaussianNoise;
using chameleon::addSaltPepperNoise;
using chameleon::BlurParams;
using chameleon::BlurType;
using chameleon::GaussianNoiseParams;
using chameleon::SaltPepperParams;

namespace {

int countExtremes(const cv::Mat &gray)
{
    cv::Mat zeros;
    cv::Mat whites;
    cv::compare(gray, 0, zeros, cv::CMP_EQ);
    cv::compare(gray, 255, whites, cv::CMP_EQ);
    return cv::countNonZero(zeros) + cv::countNonZero(whites);
}

} // namespace

TEST(NoiseTest, SaltPepperIsDeterministicForAGivenSeed)
{
    const cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(100));
    SaltPepperParams params;
    params.proportion = 0.1;

    const cv::Mat first = addSaltPepperNoise(gray, params, 42);
    const cv::Mat second = addSaltPepperNoise(gray, params, 42);
    cv::Mat diff;
    cv::absdiff(first, second, diff);
    EXPECT_EQ(cv::countNonZero(diff), 0);

    // About 10% of the pixels become 0 or 255 (collisions make it fewer)
    const double affected = double(countExtremes(first)) / first.total();
    EXPECT_GT(affected, 0.07);
    EXPECT_LT(affected, 0.11);
}

TEST(NoiseTest, SaltPepperSaltRatioIsRespected)
{
    const cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(100));
    SaltPepperParams params;
    params.proportion = 0.1;

    params.saltRatio = 1.0;  // salt only: no pixel becomes black
    double mn = 0;
    cv::minMaxLoc(addSaltPepperNoise(gray, params, 7), &mn, nullptr);
    EXPECT_DOUBLE_EQ(mn, 100.0);

    params.saltRatio = 0.0;  // pepper only: no pixel becomes white
    double mx = 0;
    cv::minMaxLoc(addSaltPepperNoise(gray, params, 7), nullptr, &mx);
    EXPECT_DOUBLE_EQ(mx, 100.0);
}

TEST(NoiseTest, SaltPepperPreservesAlpha)
{
    const cv::Mat bgra(32, 32, CV_8UC4, cv::Scalar(30, 120, 200, 77));
    SaltPepperParams params;
    params.proportion = 0.2;

    const cv::Mat out = addSaltPepperNoise(bgra, params, 3);
    ASSERT_EQ(out.type(), CV_8UC4);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x)
            EXPECT_EQ(out.at<cv::Vec4b>(y, x)[3], 77);
    }
}

TEST(NoiseTest, GaussianNoiseIsDeterministicAndCentered)
{
    const cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(100));
    GaussianNoiseParams params;
    params.mean = 0;
    params.sigma = 20;

    const cv::Mat first = addGaussianNoise(gray, params, 7);
    const cv::Mat second = addGaussianNoise(gray, params, 7);
    cv::Mat diff;
    cv::absdiff(first, second, diff);
    EXPECT_EQ(cv::countNonZero(diff), 0);

    // The added noise averages out close to the requested mean
    const cv::Scalar meanShift = cv::mean(first) - cv::mean(gray);
    EXPECT_NEAR(meanShift[0], 0.0, 3.0);
}

TEST(NoiseTest, GaussianZeroSigmaAddsAConstant)
{
    const cv::Mat gray(16, 16, CV_8UC1, cv::Scalar(100));
    GaussianNoiseParams params;
    params.mean = 10;
    params.sigma = 0;

    const cv::Mat out = addGaussianNoise(gray, params, 1);
    ASSERT_EQ(out.type(), CV_8UC1);
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x)
            EXPECT_EQ(out.at<uchar>(y, x), 110);
    }
}

TEST(NoiseTest, MedianDenoiseReducesSaltPepperNoise)
{
    const cv::Mat gray(64, 64, CV_8UC1, cv::Scalar(100));
    SaltPepperParams noise;
    noise.proportion = 0.1;
    const cv::Mat noisy = addSaltPepperNoise(gray, noise, 1);
    const int outliersBefore = countExtremes(noisy);
    ASSERT_GT(outliersBefore, 100);

    BlurParams denoise;
    denoise.type = BlurType::Median;
    denoise.kernelSize = 3;
    const cv::Mat denoised = chameleon::applyBlur(noisy, denoise);

    const int outliersAfter = countExtremes(denoised);
    EXPECT_LT(outliersAfter, outliersBefore / 4);
}

TEST(NoiseTest, EmptyInputsReturnEmpty)
{
    const cv::Mat empty;
    SaltPepperParams saltPepper;
    GaussianNoiseParams gaussian;
    EXPECT_TRUE(addSaltPepperNoise(empty, saltPepper, 1).empty());
    EXPECT_TRUE(addGaussianNoise(empty, gaussian, 1).empty());
}

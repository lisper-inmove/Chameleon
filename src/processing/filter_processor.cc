#include "processing/filter_processor.h"

#include <algorithm>

#include <opencv2/imgproc.hpp>

namespace chameleon {

cv::Mat FilterProcessor::apply(
    const cv::Mat &source, double brightness, double contrast, double saturation)  // NOLINT(bugprone-easily-swappable-parameters) - fixed convention, pinned by tests
{
    if (source.empty())
        return {};

    // Separate alpha when present so the filters do not alter it
    cv::Mat color;
    cv::Mat alpha;
    if (source.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(source, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    } else {
        color = source.clone();  // never mutate the caller's image
    }

    // Brightness/contrast: out = alpha * in + beta
    // 先改变亮度和对比度
    const double scale = 1.0 + contrast / 100.0;
    const double offset = brightness * 255.0 / 100.0;
    color.convertTo(color, -1, scale, offset);

    // Saturation via HSV; factor 0 desaturates to grayscale
    // 再转成HSV色彩空间改变饱合度
    if (saturation != 0.0 && color.channels() == 3) {
        const double factor = 1.0 + saturation / 100.0;
        cv::Mat hsv;
        cv::cvtColor(color, hsv, cv::COLOR_BGR2HSV);
        std::vector<cv::Mat> channels;
        cv::split(hsv, channels);
        channels[1].convertTo(channels[1], -1, factor, 0.0);
        cv::merge(channels, hsv);
        cv::cvtColor(hsv, color, cv::COLOR_HSV2BGR);
    }

    if (!alpha.empty()) {
        std::vector<cv::Mat> channels;
        cv::split(color, channels);
        channels.push_back(alpha);
        cv::merge(channels, color);
    }

    return color;
}

cv::Mat applyBlur(const cv::Mat &image, const BlurParams &params)
{
    if (image.empty())
        return {};

    // Blur kernels use odd sizes; the median filter additionally only
    // supports 3 and 5 for multi-channel 8-bit images
    int kernel = std::clamp(params.kernelSize, 1, 31);
    if (kernel % 2 == 0)
        ++kernel;  // blur kernels are odd-sized
    cv::Mat out;
    switch (params.type) {
    case BlurType::Box:
        cv::blur(image, out, cv::Size(kernel, kernel));
        break;
    case BlurType::Gaussian:
        cv::GaussianBlur(image, out, cv::Size(kernel, kernel),
                         params.sigmaX, params.sigmaY);
        break;
    case BlurType::Median:
        cv::medianBlur(image, out, std::clamp(kernel, 3, 5));
        break;
    case BlurType::Custom: {
        cv::Mat kernelMat(3, 3, CV_64F);
        for (int i = 0; i < 9; ++i)
            kernelMat.at<double>(i / 3, i % 3) = params.kernel[size_t(i)];
        cv::filter2D(image, out, -1, kernelMat, cv::Point(-1, -1), params.delta);
        break;
    }
    case BlurType::Bilateral:
        cv::bilateralFilter(image, out, kernel, params.sigmaColor,
                            params.sigmaSpace, params.borderType);
        break;
    case BlurType::HorizontalBox:
    case BlurType::VerticalBox:
    case BlurType::DiagonalBox: {
        // Directional mean: a kernel-size line of 1/kernel through the
        // center, zero elsewhere, applied with filter2D
        cv::Mat kernelMat = cv::Mat::zeros(kernel, kernel, CV_64F);
        const double weight = 1.0 / kernel;
        for (int i = 0; i < kernel; ++i) {
            int row = i;
            int col = i;
            if (params.type == BlurType::HorizontalBox) {
                row = kernel / 2;
                col = i;
            } else if (params.type == BlurType::VerticalBox) {
                row = i;
                col = kernel / 2;
            }
            kernelMat.at<double>(row, col) = weight;
        }
        cv::filter2D(image, out, -1, kernelMat);
        break;
    }
    }
    return out;
}

cv::Mat applyGradient(const cv::Mat &image, const GradientParams &params)
{
    if (image.empty())
        return {};

    int dx = std::clamp(params.dx, 0, 2);
    int dy = std::clamp(params.dy, 0, 2);
    if (dx == 0 && dy == 0)
        dx = 1;

    cv::Mat raw;
    switch (params.type) {
    case GradientType::Prewitt: {
        // 3x3 Prewitt kernels; dx/dy act as direction flags, both set
        // combines the two kernels
        static const double kx[9] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
        static const double ky[9] = {-1, -1, -1, 0, 0, 0, 1, 1, 1};
        cv::Mat kernelMat(3, 3, CV_64F, cv::Scalar(0));
        for (int i = 0; i < 9; ++i) {
            double value = 0.0;
            if (dx > 0)
                value += kx[i];
            if (dy > 0)
                value += ky[i];
            kernelMat.at<double>(i / 3, i % 3) = value;
        }
        cv::Mat signedResponse;
        cv::filter2D(image, signedResponse, CV_32F, kernelMat);
        cv::Mat out;
        cv::convertScaleAbs(signedResponse, out, params.scale, params.delta);
        return out;
    }
    case GradientType::Scharr: {
        // Scharr is defined for exactly one first-order direction
        if (dx > 0 && dy > 0)
            dy = 0;
        dx = std::min(dx, 1);
        dy = std::min(dy, 1);
        cv::Scharr(image, raw, CV_32F, dx, dy, params.scale, params.delta);
        break;
    }
    case GradientType::Sobel:
    default: {
        int ksize = std::clamp(params.ksize, 1, 31);
        if (ksize % 2 == 0)
            ++ksize;
        // Derivative orders are bounded by the kernel size; ksize 1 still
        // allows a single first-order direction
        const int maxOrder = std::max(1, ksize - 1);
        dx = std::min(dx, maxOrder);
        dy = std::min(dy, maxOrder);
        if (ksize == 1 && dx > 0 && dy > 0)
            dy = 0;
        cv::Sobel(image, raw, CV_32F, dx, dy, ksize, params.scale, params.delta);
        break;
    }
    }

    cv::Mat out;
    cv::convertScaleAbs(raw, out);  // signed -> displayable magnitude
    return out;
}

cv::Mat applyCanny(const cv::Mat &image, const CannyParams &params)
{
    if (image.empty())
        return {};

    cv::Mat color = image;
    cv::Mat alpha;
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    }

    cv::Mat gray;
    if (color.channels() == 1)
        gray = std::move(color);
    else
        cv::cvtColor(color, gray, cv::COLOR_BGR2GRAY);

    int aperture = std::clamp(params.apertureSize, 3, 7);
    if (aperture % 2 == 0)
        ++aperture;
    cv::Mat edges;
    cv::Canny(gray, edges, params.threshold1, params.threshold2, aperture,
              params.l2Gradient);

    // Replicate the single-channel edge map back to the input's layout so
    // the result composites uniformly (selection path, display, saving)
    cv::Mat out = edges;
    if (image.channels() == 3) {
        cv::cvtColor(edges, out, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::Mat bgr;
        cv::cvtColor(edges, bgr, cv::COLOR_GRAY2BGR);
        std::vector<cv::Mat> channels;
        cv::split(bgr, channels);
        channels.push_back(alpha);
        cv::merge(channels, out);
    }
    return out;
}

} // namespace chameleon

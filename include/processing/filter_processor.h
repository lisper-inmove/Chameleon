#pragma once

#include <array>

#include <opencv2/core.hpp>

namespace chameleon {

// Pure OpenCV filter operations. No Qt types: the input/output is
// cv::Mat (8-bit BGR / BGRA / grayscale), shared with the UI layer
// only through explicit conversion at display time.
//
// All parameters are in [-100, 100]; all zero returns a copy of the
// source image unchanged. Semantics:
//   brightness - additive offset, +100 shifts every channel by +255
//   contrast   - linear scale alpha = 1 + contrast/100, -100 collapses
//                the image to the brightness offset
//   saturation - HSV saturation factor 1 + saturation/100, -100
//                desaturates to grayscale (color channels only;
//                an alpha channel is preserved untouched)
class FilterProcessor
{
public:
    static cv::Mat apply(const cv::Mat &source, double brightness,
                         double contrast, double saturation);
};

// Convolution-family operations.
enum class BlurType {
    Box,       // cv::blur         (kernel size)
    Gaussian,  // cv::GaussianBlur (kernel size, sigmaX, sigmaY)
    Median,    // cv::medianBlur   (kernel size, clamped to 3..5)
    Custom,    // cv::filter2D     (3x3 kernel, delta)
};

struct BlurParams {
    BlurType type = BlurType::Gaussian;
    int kernelSize = 5;   // forced odd, clamped to [1, 31]; median: [3, 5]
    double sigmaX = 0.0;  // 0 = derived from the kernel size
    double sigmaY = 0.0;
    // Custom (filter2D): row-major 3x3 kernel, identity by default
    std::array<double, 9> kernel = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    double delta = 0.0;   // added to every result pixel (Custom)
};

// Applies the selected convolution. All channels (including alpha) are
// processed uniformly, matching the usual blur semantics.
cv::Mat applyBlur(const cv::Mat &image, const BlurParams &params);

} // namespace chameleon

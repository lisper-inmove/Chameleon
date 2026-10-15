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
    Box,            // cv::blur         (kernel size)
    Gaussian,       // cv::GaussianBlur (kernel size, sigmaX, sigmaY)
    Median,         // cv::medianBlur   (kernel size, clamped to 3..5)
    Custom,         // cv::filter2D     (3x3 kernel, delta)
    HorizontalBox,  // cv::filter2D     (kernel-size row filter, along x)
    VerticalBox,    // cv::filter2D     (kernel-size column filter, along y)
    DiagonalBox,    // cv::filter2D     (kernel-size diagonal filter, \)
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

// Gradient extraction.
enum class GradientType {
    Prewitt,  // hand-written 3x3 kernels applied with cv::filter2D
    Sobel,    // cv::Sobel   (ksize selectable)
    Scharr,   // cv::Scharr  (fixed 3x3; exactly one direction, first order)
};

struct GradientParams {
    GradientType type = GradientType::Sobel;
    int dx = 1;  // derivative order / direction flags (0..2)
    int dy = 0;
    int ksize = 3;    // Sobel kernel size, forced odd [1, 31]
    double scale = 1.0;
    double delta = 0.0;
};

// Computes the selected gradient. The signed response is converted to a
// displayable CV_8U magnitude (abs + saturate) of the same size; alpha is
// processed like any other channel. Prewitt uses dx/dy as direction
// flags; Scharr normalizes to exactly one first-order direction.
cv::Mat applyGradient(const cv::Mat &image, const GradientParams &params);

// Canny edge detection parameters.
struct CannyParams {
    double threshold1 = 100.0;  // hysteresis thresholds (order-insensitive)
    double threshold2 = 200.0;
    int apertureSize = 3;       // Sobel aperture, odd [3, 7]
    bool l2Gradient = false;    // use the L2 gradient norm
};

// Canny edge detection: color input is converted to luminance first, the
// edge map is then replicated back to the input's channel layout (alpha
// preserved), so the result keeps the input's type and size.
cv::Mat applyCanny(const cv::Mat &image, const CannyParams &params);

} // namespace chameleon

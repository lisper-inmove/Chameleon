#pragma once

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

} // namespace chameleon

#pragma once

#include <array>

#include <opencv2/core.hpp>

namespace chameleon {

// 256-bin histogram (0..255) of one channel of the image, computed with
// OpenCV. Channel indexes are 0-based (B=0, G=1, R=2 for BGR mats).
// Returns all zeros for an empty image or an out-of-range channel.
std::array<double, 256> computeHistogram(const cv::Mat &image, int channel);

} // namespace chameleon

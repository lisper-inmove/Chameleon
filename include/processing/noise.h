#pragma once

#include <cstdint>

#include <opencv2/core.hpp>

namespace chameleon {

// Noise generation (OpenCV-only, Qt-free). A nonzero seed makes the
// result reproducible; seed 0 draws a fresh time-based seed.

struct SaltPepperParams {
    double proportion = 0.02;  // fraction of pixels affected, [0, 1]
    double saltRatio = 0.5;    // share of white (salt) vs black (pepper)
};

struct GaussianNoiseParams {
    double mean = 0.0;
    double sigma = 20.0;
};

// Replaces random pixels with 0 (pepper) or 255 (salt) on the color
// channels; an alpha channel is left untouched.
cv::Mat addSaltPepperNoise(const cv::Mat &image, const SaltPepperParams &params,
                           uint64_t seed = 0);

// Adds independent per-channel Gaussian noise; an alpha channel is left
// untouched.
cv::Mat addGaussianNoise(const cv::Mat &image, const GaussianNoiseParams &params,
                         uint64_t seed = 0);

} // namespace chameleon

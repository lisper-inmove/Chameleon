#include "processing/noise.h"

#include <algorithm>
#include <vector>

namespace chameleon {

namespace {

uint64_t resolveSeed(uint64_t seed)
{
    return seed != 0 ? seed : uint64_t(cv::getTickCount());
}

// Splits a possible alpha channel off; returns the color part in `color`
// and the alpha in `alpha` (empty when the input has none)
void splitAlpha(const cv::Mat &image, cv::Mat &color, cv::Mat &alpha)  // NOLINT(bugprone-easily-swappable-parameters) - fixed order, one call site
{
    color = image;
    alpha = cv::Mat();
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    }
}

void mergeAlpha(cv::Mat &color, const cv::Mat &alpha)
{
    if (alpha.empty())
        return;
    std::vector<cv::Mat> channels;
    cv::split(color, channels);
    channels.push_back(alpha);
    cv::merge(channels, color);
}

} // namespace

cv::Mat addSaltPepperNoise(const cv::Mat &image, const SaltPepperParams &params,
                           uint64_t seed)
{
    if (image.empty())
        return {};

    cv::Mat out = image.clone();
    cv::RNG rng(resolveSeed(seed));
    const double proportion = std::clamp(params.proportion, 0.0, 1.0);
    const double saltRatio = std::clamp(params.saltRatio, 0.0, 1.0);
    const int count = int(double(image.total()) * proportion);
    // Alpha (the 4th byte) is preserved; color bytes get salt or pepper
    const int colorBytes = image.channels() == 4 ? 3 : image.channels();

    for (int i = 0; i < count; ++i) {
        const int y = rng.uniform(0, out.rows);
        const int x = rng.uniform(0, out.cols);
        const uchar value = rng.uniform(0.0, 1.0) < saltRatio ? 255 : 0;
        uchar *pixel = out.ptr(y) + size_t(x) * out.elemSize();
        for (int b = 0; b < colorBytes; ++b)
            pixel[b] = value;
    }
    return out;
}

cv::Mat addGaussianNoise(const cv::Mat &image, const GaussianNoiseParams &params,
                         uint64_t seed)
{
    if (image.empty())
        return {};

    cv::Mat color;
    cv::Mat alpha;
    splitAlpha(image, color, alpha);

    cv::RNG rng(resolveSeed(seed));
    cv::Mat noise(color.size(), CV_32F);
    rng.fill(noise, cv::RNG::NORMAL, params.mean, params.sigma);

    cv::Mat floatColor;
    color.convertTo(floatColor, CV_32F);
    floatColor += noise;

    cv::Mat out;
    floatColor.convertTo(out, color.type());  // saturates to 8 bits
    mergeAlpha(out, alpha);
    return out;
}

} // namespace chameleon

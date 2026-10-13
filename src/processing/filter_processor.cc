#include "processing/filter_processor.h"

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

} // namespace chameleon

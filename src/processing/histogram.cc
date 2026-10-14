#include "processing/histogram.h"

#include <opencv2/imgproc.hpp>

namespace chameleon {

std::array<double, 256> computeHistogram(const cv::Mat &image, int channel)
{
    std::array<double, 256> bins{};
    if (image.empty() || channel < 0 || channel >= image.channels())
        return bins;

    const int channels[] = {channel};
    int histSize[] = {256};
    float range[] = {0, 256};
    const float *ranges[] = {range};
    cv::Mat hist;
    cv::calcHist(&image, 1, channels, cv::Mat(), hist, 1, histSize, ranges, true);
    for (int i = 0; i < 256; ++i)
        bins[i] = hist.at<float>(i);
    return bins;
}

} // namespace chameleon

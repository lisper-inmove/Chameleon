#include "processing/binary.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace chameleon {

namespace {

int toCvAdaptiveMethod(AdaptiveThresholdMethod method)
{
    return method == AdaptiveThresholdMethod::Gaussian
               ? cv::ADAPTIVE_THRESH_GAUSSIAN_C
               : cv::ADAPTIVE_THRESH_MEAN_C;
}

int toCvThresholdType(ThresholdType type)
{
    switch (type) {
    case ThresholdType::BinaryInv:
        return cv::THRESH_BINARY_INV;
    case ThresholdType::Trunc:
        return cv::THRESH_TRUNC;
    case ThresholdType::ToZero:
        return cv::THRESH_TOZERO;
    case ThresholdType::ToZeroInv:
        return cv::THRESH_TOZERO_INV;
    case ThresholdType::Binary:
    default:
        return cv::THRESH_BINARY;
    }
}

} // namespace

cv::Mat applyThreshold(const cv::Mat &image, const ThresholdParams &params,
                       double *computedThresh)
{
    if (computedThresh)
        *computedThresh = params.thresh;
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

    cv::Mat out;
    if (params.method == ThresholdMethod::Adaptive) {
        // adaptiveThreshold needs a single channel: threshold the
        // luminance and replicate the result back to the input layout
        cv::Mat gray = color;
        if (color.channels() != 1)
            cv::cvtColor(color, gray, cv::COLOR_BGR2GRAY);
        int blockSize = std::clamp(params.blockSize, 3, 99);
        if (blockSize % 2 == 0)
            ++blockSize;
        const int binaryType = params.type == ThresholdType::BinaryInv
                                   ? cv::THRESH_BINARY_INV
                                   : cv::THRESH_BINARY;
        cv::Mat local;
        cv::adaptiveThreshold(gray, local, params.maxval,
                              toCvAdaptiveMethod(params.adaptiveMethod),
                              binaryType, blockSize, params.c);
        out = local;
        if (color.channels() == 3)
            cv::cvtColor(local, out, cv::COLOR_GRAY2BGR);
    } else {
        // THRESH_OTSU / THRESH_TRIANGLE are combined as flags (the
        // OpenCV idiom, flagged spuriously by the signed-bitwise check)
        int type = toCvThresholdType(params.type);  // NOLINT(bugprone-signed-bitwise)
        cv::Mat work = color;
        if (params.method == ThresholdMethod::Otsu)
            type = type | cv::THRESH_OTSU;  // NOLINT(bugprone-signed-bitwise)
        else if (params.method == ThresholdMethod::Triangle)
            type = type | cv::THRESH_TRIANGLE;  // NOLINT(bugprone-signed-bitwise)
        if (params.method != ThresholdMethod::Fixed && color.channels() != 1) {
            cv::cvtColor(color, work, cv::COLOR_BGR2GRAY);
        }
        cv::Mat thresholded;
        const double value =
            cv::threshold(work, thresholded, params.thresh, params.maxval, type);
        if (computedThresh)
            *computedThresh = value;
        out = thresholded;
        if (work.channels() == 1 && color.channels() == 3)
            cv::cvtColor(thresholded, out, cv::COLOR_GRAY2BGR);
    }

    if (!alpha.empty()) {
        std::vector<cv::Mat> channels;
        cv::split(out, channels);
        channels.push_back(alpha);
        cv::merge(channels, out);
    }
    return out;
}

namespace {

cv::Mat toSingleChannel(const cv::Mat &image)
{
    cv::Mat color = image;
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    }
    if (color.channels() == 1)
        return color;
    cv::Mat gray;
    cv::cvtColor(color, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

} // namespace

ConnectedComponentsResult connectedComponents(const cv::Mat &image,
                                              const ConnectedComponentsParams &params)
{
    ConnectedComponentsResult result;
    if (image.empty())
        return result;

    const cv::Mat gray = toSingleChannel(image);
    const int connectivity = params.connectivity == 4 ? 4 : 8;

    if (params.withStats) {
        cv::Mat stats;
        cv::Mat centroids;
        result.count = cv::connectedComponentsWithStats(gray, result.labels, stats,
                                                        centroids, connectivity);
        for (int label = 0; label < result.count; ++label) {
            result.boundingBoxes.emplace_back(stats.at<int>(label, cv::CC_STAT_LEFT),
                                              stats.at<int>(label, cv::CC_STAT_TOP),
                                              stats.at<int>(label, cv::CC_STAT_WIDTH),
                                              stats.at<int>(label, cv::CC_STAT_HEIGHT));
            result.areas.push_back(stats.at<int>(label, cv::CC_STAT_AREA));
            result.centroids.emplace_back(centroids.at<double>(label, 0),
                                          centroids.at<double>(label, 1));
        }
    } else {
        result.count = cv::connectedComponents(gray, result.labels, connectivity);
    }
    return result;
}

cv::Mat colorizeLabels(const cv::Mat &labels, bool annotateIds)
{
    if (labels.empty())
        return {};

    double maxLabel = 0;
    cv::minMaxLoc(labels, nullptr, &maxLabel);
    const int labelCount = int(maxLabel);

    // Distinct colors via golden-ratio hue stepping
    cv::Mat hsv(1, labelCount, CV_8UC3);  // NOLINT(bugprone-signed-bitwise) - CV_8UC3 macro expansion
    for (int label = 1; label <= labelCount; ++label) {
        const double hue = std::fmod(label * 0.618033988749895, 1.0) * 180.0;
        hsv.at<cv::Vec3b>(0, label - 1) = cv::Vec3b(uchar(hue), 200, 255);
    }
    cv::Mat bgr;
    if (labelCount > 0)
        cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR);

    cv::Mat out(labels.size(), CV_8UC3, cv::Scalar(0, 0, 0));  // NOLINT(bugprone-signed-bitwise) - CV_8UC3 macro; background filled black
    // Centroid accumulation for the optional ID annotation
    std::vector<cv::Point2d> sums(size_t(labelCount) + 1, cv::Point2d(0, 0));
    std::vector<int> counts(size_t(labelCount) + 1, 0);
    for (int y = 0; y < labels.rows; ++y) {
        const int *row = labels.ptr<int>(y);
        cv::Vec3b *outRow = out.ptr<cv::Vec3b>(y);
        for (int x = 0; x < labels.cols; ++x) {
            const int label = row[x];
            if (label > 0 && labelCount > 0) {
                outRow[x] = bgr.at<cv::Vec3b>(0, label - 1);
                sums[size_t(label)] += cv::Point2d(x, y);
                ++counts[size_t(label)];
            }
        }
    }

    if (annotateIds) {
        const double fontScale = 0.45;
        const int thickness = 1;
        for (int label = 1; label <= labelCount; ++label) {
            if (counts[size_t(label)] == 0)
                continue;
            const cv::Point2d center = sums[size_t(label)] * (1.0 / counts[size_t(label)]);
            const std::string text = std::to_string(label);
            int baseline = 0;
            const cv::Size size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX,
                                                  fontScale, thickness, &baseline);
            const cv::Point org(int(std::lround(center.x - size.width / 2.0)),
                                int(std::lround(center.y + size.height / 2.0)));
            // Dark outline keeps the ID readable on any component color
            cv::putText(out, text, org, cv::FONT_HERSHEY_SIMPLEX, fontScale,
                        cv::Scalar(0, 0, 0), thickness + 1, cv::LINE_AA);
            cv::putText(out, text, org, cv::FONT_HERSHEY_SIMPLEX, fontScale,
                        cv::Scalar(255, 255, 255), thickness, cv::LINE_AA);
        }
    }
    return out;
}

} // namespace chameleon

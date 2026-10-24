#include "processing/binary.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/geometry/2d.hpp>  // OpenCV 5: contourArea
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

namespace {

struct SortedContours {
    std::vector<std::vector<cv::Point>> points;
    std::vector<ContourInfo> infos;
};

cv::Scalar outlineColor(int channels)
{
    if (channels == 3)
        return {0, 255, 0};  // green
    if (channels == 4)
        return {0, 255, 0, 255};
    return {255};  // white on grayscale input
}

cv::Scalar fitColor(int channels)
{
    if (channels == 3)
        return {0, 0, 255};  // red
    if (channels == 4)
        return {0, 0, 255, 255};
    return {255};
}

SortedContours findSortedContours(const cv::Mat &image,
                                  const ContourParams &params)
{
    SortedContours result;

    cv::Mat gray;
    switch (image.channels()) {
    case 1:
        gray = image;
        break;
    case 3:
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        break;
    default:
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
        break;
    }

    cv::Mat binary;
    const int type = cv::THRESH_BINARY  // NOLINT(bugprone-signed-bitwise) - OpenCV flags
            | (params.otsu ? cv::THRESH_OTSU : 0);
    cv::threshold(gray, binary, params.threshold, 255, type);
    if (params.invert)
        cv::bitwise_not(binary, binary);

    std::vector<std::vector<cv::Point>> found;
    cv::findContours(binary, found, params.retrieval, params.approximation,
                     params.offset);

    // Number top-to-bottom, left-to-right: cv::findContours does not
    // guarantee that order (RETR_EXTERNAL returned bottom shapes first)
    const size_t contourCount = found.size();
    std::vector<cv::Rect> boxes;
    boxes.reserve(contourCount);
    for (const auto &contour : found)
        boxes.push_back(cv::boundingRect(contour));
    std::vector<size_t> order(contourCount);
    std::iota(order.begin(), order.end(), size_t(0));
    std::stable_sort(order.begin(), order.end(), [&boxes](size_t a, size_t b) {
        if (boxes[a].y != boxes[b].y)
            return boxes[a].y < boxes[b].y;
        return boxes[a].x < boxes[b].x;
    });

    result.points.reserve(contourCount);
    result.infos.reserve(contourCount);
    for (size_t rank = 0; rank < contourCount; ++rank) {
        const size_t source = order[rank];
        ContourInfo info;
        info.index = int(rank) + 1;
        info.area = cv::contourArea(found[source]);
        info.perimeter = cv::arcLength(found[source], true);
        info.boundingBox = boxes[source];
        result.infos.push_back(info);
        result.points.push_back(std::move(found[source]));
    }
    return result;
}

void drawContourNumbers(cv::Mat &image, const std::vector<ContourInfo> &infos)
{
    for (const ContourInfo &info : infos) {
        const std::string text = std::to_string(info.index);
        const int x = std::clamp(info.boundingBox.x + 2, 0,
                                 std::max(0, image.cols - 1));
        const int y = std::clamp(info.boundingBox.y + 12, 0,
                                 std::max(0, image.rows - 1));
        cv::putText(image, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX,
                    0.4, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
        cv::putText(image, text, cv::Point(x, y), cv::FONT_HERSHEY_SIMPLEX,
                    0.4, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }
}

void drawOutline(cv::Mat &image, const SortedContours &found,
                 const ContourParams &params)
{
    if (found.points.empty())
        return;
    int contourIndex = -1;  // draw all
    if (params.largestOnly) {
        double bestArea = -1.0;
        for (size_t i = 0; i < found.points.size(); ++i) {
            const double area = std::abs(cv::contourArea(found.points[i]));
            if (area > bestArea) {
                bestArea = area;
                contourIndex = int(i);
            }
        }
    }
    cv::drawContours(image, found.points, contourIndex,
                     outlineColor(image.channels()), params.thickness,
                     params.lineType);
    drawContourNumbers(image, found.infos);
}

void fitContour(cv::Mat &image, const std::vector<cv::Point> &contour,
                const ContourFitParams &params, ContourFitInfo &info)
{
    switch (params.type) {
    case ContourFitType::Ellipse:
        if (contour.size() < 5)
            return;  // cv::fitEllipse needs at least five points
        info.ellipse = cv::fitEllipse(contour);
        info.valid = true;
        cv::ellipse(image, info.ellipse, fitColor(image.channels()),
                    params.thickness, cv::LINE_AA);
        break;
    case ContourFitType::Line: {
        if (contour.size() < 2)
            return;
        cv::Vec4f line;
        cv::fitLine(contour, line, params.distType, params.param, params.reps,
                    params.aeps);
        info.linePoint = cv::Point2f(line[2], line[3]);
        info.lineDirection = cv::Point2f(line[0], line[1]);
        info.valid = true;
        // Draw the segment spanning the contour's projection on the line
        double tMin = std::numeric_limits<double>::max();
        double tMax = -tMin;
        for (const cv::Point &point : contour) {
            const double t = (double(point.x) - line[2]) * line[0]
                    + (double(point.y) - line[3]) * line[1];
            tMin = std::min(tMin, t);
            tMax = std::max(tMax, t);
        }
        cv::line(image, info.linePoint + info.lineDirection * float(tMin),
                 info.linePoint + info.lineDirection * float(tMax),
                 fitColor(image.channels()), params.thickness, cv::LINE_AA);
        break;
    }
    case ContourFitType::Polygon: {
        std::vector<cv::Point> approx;
        cv::approxPolyDP(contour, approx, params.epsilon, params.closed);
        info.vertexCount = int(approx.size());
        info.polygonArea = cv::contourArea(approx);
        info.valid = !approx.empty();
        if (info.valid)
            cv::polylines(image, approx, params.closed,
                          fitColor(image.channels()), params.thickness,
                          cv::LINE_AA);
        break;
    }
    }
}

} // namespace

ContourResult findAndDrawContours(const cv::Mat &image,
                                  const ContourParams &params)
{
    ContourResult result;
    if (image.empty())
        return result;

    SortedContours found = findSortedContours(image, params);
    result.image = image.clone();
    drawOutline(result.image, found, params);
    result.contours = std::move(found.points);
    result.infos = std::move(found.infos);
    result.count = int(result.contours.size());
    return result;
}

ContourFitResult findAndFitContours(const cv::Mat &image,
                                    const ContourParams &contourParams,
                                    const ContourFitParams &fitParams)
{
    ContourFitResult result;
    if (image.empty())
        return result;

    SortedContours found = findSortedContours(image, contourParams);
    result.image = image.clone();
    // The fit view always outlines and numbers every contour, even when
    // the plain drawing option limits the outline to the largest one
    if (!found.points.empty()) {
        cv::drawContours(result.image, found.points, -1,
                         outlineColor(result.image.channels()),
                         contourParams.thickness, contourParams.lineType);
        drawContourNumbers(result.image, found.infos);
    }

    result.contourInfos = found.infos;
    result.fits.reserve(found.points.size());
    for (size_t i = 0; i < found.points.size(); ++i) {
        ContourFitInfo info;
        info.index = found.infos[i].index;
        try {
            fitContour(result.image, found.points[i], fitParams, info);
        } catch (const cv::Exception &) {
            info = ContourFitInfo{};  // degenerate contour: report invalid
            info.index = found.infos[i].index;
        }
        result.fits.push_back(info);
    }
    return result;
}

} // namespace chameleon

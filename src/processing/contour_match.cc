#include "processing/contour_match.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include <opencv2/imgproc.hpp>

namespace chameleon {

namespace {

cv::Scalar outlineColor(int channels)
{
    if (channels == 3)
        return {0, 255, 0};  // green
    if (channels == 4)
        return {0, 255, 0, 255};
    return {255};
}

cv::Scalar bestColor(int channels)
{
    if (channels == 3)
        return {0, 0, 255};  // red
    if (channels == 4)
        return {0, 0, 255, 255};
    return {255};
}

void drawNumbers(cv::Mat &image, const std::vector<ContourInfo> &infos)
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

} // namespace

ContourMatchResult matchContours(const cv::Mat &image,
                                 const cv::Mat &templateImage,
                                 const ContourMatchParams &params)
{
    ContourMatchResult result;
    if (image.empty() || templateImage.empty())
        return result;

    const DetectedContours query = detectContours(image, params.contourParams);
    const DetectedContours reference =
        detectContours(templateImage, params.contourParams);

    result.image = image.clone();
    if (query.contours.empty() || reference.contours.empty())
        return result;

    // The largest template contour is the reference shape
    size_t referenceIndex = 0;
    double referenceArea = -1.0;
    for (size_t i = 0; i < reference.contours.size(); ++i) {
        const double area = std::abs(cv::contourArea(reference.contours[i]));
        if (area > referenceArea) {
            referenceArea = area;
            referenceIndex = i;
        }
    }

    cv::drawContours(result.image, query.contours, -1,
                     outlineColor(result.image.channels()), 2, cv::LINE_AA);
    drawNumbers(result.image, query.infos);

    result.matches.reserve(query.contours.size());
    size_t bestContourIndex = 0;
    double bestScore = std::numeric_limits<double>::max();
    bool haveBest = false;
    for (size_t i = 0; i < query.contours.size(); ++i) {
        ContourMatchInfo info;
        info.index = query.infos[i].index;

        // Requirement: obtain the raw data with cv::moments, then the
        // seven Hu invariants with cv::HuMoments
        info.moments = cv::moments(query.contours[i]);
        cv::Mat hu;
        cv::HuMoments(info.moments, hu);
        for (int k = 0; k < 7; ++k)
            info.hu[size_t(k)] = hu.at<double>(k, 0);

        try {
            info.score = cv::matchShapes(query.contours[i],
                                         reference.contours[referenceIndex],
                                         params.method, params.parameter);
        } catch (const cv::Exception &) {
            info.score = std::numeric_limits<double>::max();
        }
        if (!std::isfinite(info.score))
            info.score = std::numeric_limits<double>::max();

        if (info.score < bestScore) {
            bestScore = info.score;
            bestContourIndex = i;
            result.bestIndex = info.index;
            haveBest = true;
        }
        result.matches.push_back(info);
    }

    // Present the results by similarity: lowest matchShapes score first
    std::stable_sort(result.matches.begin(), result.matches.end(),
                     [](const ContourMatchInfo &a, const ContourMatchInfo &b) {
                         return a.score < b.score;
                     });

    if (haveBest) {
        cv::drawContours(result.image, query.contours, int(bestContourIndex),
                         bestColor(result.image.channels()), 3, cv::LINE_AA);
    }
    return result;
}

} // namespace chameleon

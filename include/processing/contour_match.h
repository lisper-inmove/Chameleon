#pragma once

#include <array>
#include <vector>

#include <opencv2/core.hpp>

#include "processing/binary.h"

namespace chameleon {

// Shape matching between the contours of the current image and a
// template image: cv::moments -> cv::HuMoments -> cv::matchShapes.

struct ContourMatchParams {
    // cv::matchShapes
    int method = cv::CONTOURS_MATCH_I1;  // I1 / I2 / I3
    double parameter = 0.0;              // method-specific parameter
    // Contour detection applied to both the query and the template
    ContourParams contourParams;
};

struct ContourMatchInfo {
    int index = 0;          // query contour number (1-based)
    double score = 0.0;     // cv::matchShapes, lower = more similar
    cv::Moments moments;    // cv::moments of the query contour
    std::array<double, 7> hu{};  // cv::HuMoments
};

struct ContourMatchResult {
    std::vector<ContourMatchInfo> matches;
    int bestIndex = 0;  // 1-based number of the closest contour; 0 = none
    // Query image with the contours drawn (green); the best match is
    // highlighted in red (white on grayscale input)
    cv::Mat image;
};

// Detects contours in the query (largest template contour is the
// reference), computes moments/Hu moments per query contour and scores
// them with cv::matchShapes.
ContourMatchResult matchContours(const cv::Mat &image,
                                 const cv::Mat &templateImage,
                                 const ContourMatchParams &params);

} // namespace chameleon

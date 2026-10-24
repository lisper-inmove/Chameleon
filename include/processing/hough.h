#pragma once

#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace chameleon {

// Hough transform shape detection (OpenCV-only, Qt-free).

enum class HoughMode { Lines, Circles };

struct HoughLinesParams {
    // cv::HoughLines
    double rho = 1.0;           // distance resolution (pixels)
    double thetaDegrees = 1.0;  // angle resolution (degrees)
    int threshold = 150;        // accumulator threshold
    double srn = 0.0;           // multi-scale divisor for rho (0 = classic)
    double stn = 0.0;           // multi-scale divisor for theta
    double minThetaDegrees = 0.0;
    double maxThetaDegrees = 180.0;
    bool useEdgeVal = false;  // OpenCV 5: use edge values in the accumulator
    // Canny preprocessing
    double cannyThreshold1 = 100.0;
    double cannyThreshold2 = 200.0;
    // Drawing
    int thickness = 2;
};

struct HoughCirclesParams {
    // cv::HoughCircles
    int method = cv::HOUGH_GRADIENT;
    double dp = 1.0;
    double minDist = 20.0;
    double param1 = 100.0;
    double param2 = 30.0;
    int minRadius = 0;
    int maxRadius = 0;
    // Drawing
    int thickness = 2;
};

struct HoughLineInfo {
    int index = 0;  // 1-based
    double rho = 0.0;
    double thetaDegrees = 0.0;
};

struct HoughCircleInfo {
    int index = 0;  // 1-based
    cv::Point2f center{0.0f, 0.0f};
    float radius = 0.0f;
};

struct HoughResult {
    std::vector<HoughLineInfo> lines;
    std::vector<HoughCircleInfo> circles;
    // Input with the detected shapes drawn in red (white on grayscale)
    cv::Mat image;
};

// Grayscale -> Canny edges -> cv::HoughLines; the detected lines are
// drawn across the whole image.
HoughResult houghLines(const cv::Mat &image, const HoughLinesParams &params);

// Grayscale -> cv::HoughCircles; each circle is drawn as an outline.
HoughResult houghCircles(const cv::Mat &image,
                         const HoughCirclesParams &params);

} // namespace chameleon

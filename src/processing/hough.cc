#include "processing/hough.h"

#include <cmath>

#include <opencv2/imgproc.hpp>

namespace chameleon {

namespace {

cv::Mat toGray(const cv::Mat &image)
{
    if (image.channels() == 1)
        return image;
    cv::Mat gray;
    cv::cvtColor(image, gray, image.channels() == 4 ? cv::COLOR_BGRA2GRAY
                                                    : cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Scalar drawColor(int channels)
{
    if (channels == 3)
        return {0, 0, 255};  // red
    if (channels == 4)
        return {0, 0, 255, 255};
    return {255};  // white on grayscale input
}

constexpr double kPi = 3.14159265358979323846;

} // namespace

HoughResult houghLines(const cv::Mat &image, const HoughLinesParams &params)
{
    HoughResult result;
    if (image.empty())
        return result;

    cv::Mat edges;
    cv::Canny(toGray(image), edges, params.cannyThreshold1,
              params.cannyThreshold2);

    std::vector<cv::Vec2f> lines;
    try {
        cv::HoughLines(edges, lines, params.rho,
                       params.thetaDegrees * kPi / 180.0, params.threshold,
                       params.srn, params.stn,
                       params.minThetaDegrees * kPi / 180.0,
                       params.maxThetaDegrees * kPi / 180.0,
                       params.useEdgeVal);
    } catch (const cv::Exception &) {
        return result;  // invalid parameter combination: leave untouched
    }

    result.image = image.clone();
    const cv::Scalar color = drawColor(image.channels());
    const float reach =
        float(std::hypot(double(image.cols), double(image.rows)));
    result.lines.reserve(lines.size());
    for (const cv::Vec2f &line : lines) {
        HoughLineInfo info;
        info.index = int(result.lines.size()) + 1;
        info.rho = line[0];
        info.thetaDegrees = double(line[1]) * 180.0 / kPi;
        result.lines.push_back(info);

        // Standard (rho, theta) -> segment drawing: extend the point on
        // the normal along the line direction by the image diagonal
        const double a = std::cos(line[1]);
        const double b = std::sin(line[1]);
        const cv::Point2f onNormal(float(info.rho * a), float(info.rho * b));
        const cv::Point2f direction(float(-b), float(a));
        cv::line(result.image, onNormal - direction * reach,
                 onNormal + direction * reach, color, params.thickness,
                 cv::LINE_AA);
    }
    return result;
}

HoughResult houghCircles(const cv::Mat &image,
                         const HoughCirclesParams &params)
{
    HoughResult result;
    if (image.empty())
        return result;

    std::vector<cv::Vec3f> circles;
    try {
        cv::HoughCircles(toGray(image), circles, params.method, params.dp,
                         params.minDist, params.param1, params.param2,
                         params.minRadius, params.maxRadius);
    } catch (const cv::Exception &) {
        return result;  // invalid parameter combination: leave untouched
    }

    result.image = image.clone();
    const cv::Scalar color = drawColor(image.channels());
    result.circles.reserve(circles.size());
    for (const cv::Vec3f &circle : circles) {
        HoughCircleInfo info;
        info.index = int(result.circles.size()) + 1;
        info.center = cv::Point2f(circle[0], circle[1]);
        info.radius = circle[2];
        result.circles.push_back(info);

        cv::circle(result.image, cv::Point(cvRound(circle[0]), cvRound(circle[1])),
                   cvRound(circle[2]), color, params.thickness, cv::LINE_AA);
    }
    return result;
}

} // namespace chameleon

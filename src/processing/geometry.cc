#include "processing/geometry.h"

#include <algorithm>
#include <cmath>

#include <opencv2/geometry/2d.hpp>  // OpenCV 5: rotation/perspective helpers
#include <opencv2/imgproc.hpp>

namespace chameleon {

cv::Mat rotateImage(const cv::Mat &image, double angleDegrees)
{
    if (image.empty())
        return {};

    // Exact path for 90-degree multiples: lossless and dimension-swapping.
    // Positive angles are counter-clockwise, consistent with
    // getRotationMatrix2D below and with the dialog hint.
    const double normalized = std::fmod(angleDegrees, 360.0);
    if (std::abs(std::fmod(normalized, 90.0)) < 1e-6) {
        int quarterTurns = int(std::lround(normalized / 90.0)) % 4;
        if (quarterTurns < 0)
            quarterTurns += 4;
        cv::Mat out;
        switch (quarterTurns) {
        case 1:
            cv::rotate(image, out, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        case 2:
            cv::rotate(image, out, cv::ROTATE_180);
            break;
        case 3:
            cv::rotate(image, out, cv::ROTATE_90_CLOCKWISE);
            break;
        default:
            out = image.clone();
            break;
        }
        return out;
    }

    const cv::Point2f center(float(image.cols) / 2.0f, float(image.rows) / 2.0f);
    cv::Mat rotation = cv::getRotationMatrix2D(center, angleDegrees, 1.0);

    // Expanded bounding box of the rotated image
    const double radians = angleDegrees * CV_PI / 180.0;
    const double c = std::abs(std::cos(radians));
    const double s = std::abs(std::sin(radians));
    const int width = std::max(1, int(std::lround(image.cols * c + image.rows * s)));
    const int height = std::max(1, int(std::lround(image.cols * s + image.rows * c)));

    rotation.at<double>(0, 2) += width / 2.0 - center.x;
    rotation.at<double>(1, 2) += height / 2.0 - center.y;

    cv::Mat out;
    cv::warpAffine(image, out, rotation, cv::Size(width, height), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0, 255));
    return out;
}

cv::Mat flipImage(const cv::Mat &image, FlipDirection direction)
{
    if (image.empty())
        return {};
    int flipCode = 1;  // horizontal
    if (direction == FlipDirection::Vertical)
        flipCode = 0;
    else if (direction == FlipDirection::Both)
        flipCode = -1;
    cv::Mat out;
    cv::flip(image, out, flipCode);
    return out;
}

cv::Mat resizeImage(const cv::Mat &image, int width, int height,
                    ResampleMethod method)
{
    if (image.empty() || width <= 0 || height <= 0)
        return {};
    int interpolation = cv::INTER_LINEAR;
    switch (method) {
    case ResampleMethod::Nearest:
        interpolation = cv::INTER_NEAREST;
        break;
    case ResampleMethod::Cubic:
        interpolation = cv::INTER_CUBIC;
        break;
    case ResampleMethod::Linear:
    default:
        interpolation = cv::INTER_LINEAR;
        break;
    }
    cv::Mat out;
    cv::resize(image, out, cv::Size(width, height), 0, 0, interpolation);
    return out;
}

cv::Mat perspectiveCorrect(const cv::Mat &image,
                           const std::array<cv::Point2f, 4> &quad)
{
    if (image.empty())
        return {};

    const double widthTop = cv::norm(quad[1] - quad[0]);
    const double widthBottom = cv::norm(quad[2] - quad[3]);
    const double heightLeft = cv::norm(quad[3] - quad[0]);
    const double heightRight = cv::norm(quad[2] - quad[1]);
    const int width = std::max(1, int(std::lround(std::max(widthTop, widthBottom))));
    const int height = std::max(1, int(std::lround(std::max(heightLeft, heightRight))));

    const cv::Point2f source[4] = {quad[0], quad[1], quad[2], quad[3]};
    const cv::Point2f destination[4] = {{0.0f, 0.0f},
                                        {float(width - 1), 0.0f},
                                        {float(width - 1), float(height - 1)},
                                        {0.0f, float(height - 1)}};
    const cv::Mat transform = cv::getPerspectiveTransform(source, destination);

    cv::Mat out;
    cv::warpPerspective(image, out, transform, cv::Size(width, height));
    return out;
}

cv::Mat adjustCanvas(const cv::Mat &image, int width, int height)
{
    if (image.empty() || width <= 0 || height <= 0)
        return {};

    if (width >= image.cols && height >= image.rows) {
        const cv::Scalar fill = image.channels() == 4 ? cv::Scalar(0, 0, 0, 255)
                                                      : cv::Scalar(0);
        cv::Mat out(height, width, image.type(), fill);
        image.copyTo(out(cv::Rect(0, 0, image.cols, image.rows)));
        return out;
    }
    return image(cv::Rect(0, 0, std::min(width, image.cols),
                          std::min(height, image.rows))).clone();
}

} // namespace chameleon

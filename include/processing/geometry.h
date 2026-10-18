#pragma once

#include <array>

#include <opencv2/core.hpp>

namespace chameleon {

// Geometric transforms (OpenCV-only, Qt-free).

enum class FlipDirection { Horizontal, Vertical, Both };
enum class ResampleMethod { Nearest, Linear, Cubic };

enum class GeometryOperation { Crop, Rotate, Flip, Resize, Perspective, Canvas };

struct GeometryParams {
    GeometryOperation operation = GeometryOperation::Rotate;
    // Rotate (degrees, positive = counter-clockwise)
    double angle = 90.0;
    // Flip
    FlipDirection flip = FlipDirection::Horizontal;
    // Resize / Canvas
    int width = 0;
    int height = 0;
    ResampleMethod resample = ResampleMethod::Linear;
    // Perspective: TL, TR, BR, BL in image pixels
    std::array<cv::Point2f, 4> quad{};
};

// Rotates by any angle; 90-degree multiples use the exact cv::rotate
// path, other angles use warpAffine with the canvas expanded to fit.
cv::Mat rotateImage(const cv::Mat &image, double angleDegrees);

cv::Mat flipImage(const cv::Mat &image, FlipDirection direction);

// Resizes to the given size (clamped to >= 1) with the chosen interpolation.
cv::Mat resizeImage(const cv::Mat &image, int width, int height,
                    ResampleMethod method);

// Maps the quadrilateral (TL, TR, BR, BL) onto a straightened rectangle
// whose size is the longest horizontal/vertical quad edges.
cv::Mat perspectiveCorrect(const cv::Mat &image,
                           const std::array<cv::Point2f, 4> &quad);

// Resizes the canvas: the image stays top-left, extra space is filled
// (opaque black) and out-of-canvas areas are cropped.
cv::Mat adjustCanvas(const cv::Mat &image, int width, int height);

} // namespace chameleon

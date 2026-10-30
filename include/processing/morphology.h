#pragma once

#include <opencv2/core.hpp>

namespace chameleon {

// Morphological operations (OpenCV-only, Qt-free).

enum class MorphologyOperation {
    Dilate,
    Erode,
    Open,
    Close,
    BasicGradient,     // dilation - erosion
    InternalGradient,  // source - erosion
    ExternalGradient,  // dilation - source
};
enum class MorphologyShape { Rect, Cross, Ellipse };
// Opening/closing can be computed by cv::morphologyEx or by chaining
// cv::erode and cv::dilate (the two are equivalent)
enum class MorphologyMethod { MorphologyEx, Combined };

struct MorphologyParams {
    MorphologyOperation operation = MorphologyOperation::Dilate;
    MorphologyMethod method = MorphologyMethod::MorphologyEx;
    // Structuring element (cv::getStructuringElement)
    MorphologyShape shape = MorphologyShape::Rect;
    int kernelWidth = 3;
    int kernelHeight = 3;
    // cv::erode / cv::dilate / cv::morphologyEx parameters
    cv::Point anchor{-1, -1};  // (-1, -1) = kernel centre
    int iterations = 1;
    int borderType = cv::BORDER_CONSTANT;
    double borderValue = 0.0;  // applied to every channel
};

// Applies the chosen operation with a structuring element built from the
// shape/size parameters. Open/close use either cv::morphologyEx or the
// erode+dilate chain (params.method). Multi-channel input is processed
// per channel and an alpha channel is preserved unchanged.
cv::Mat applyMorphology(const cv::Mat &image, const MorphologyParams &params);

} // namespace chameleon

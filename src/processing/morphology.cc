#include "processing/morphology.h"

#include <algorithm>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace chameleon {

namespace {

int toShape(MorphologyShape shape)
{
    switch (shape) {
    case MorphologyShape::Cross:
        return cv::MORPH_CROSS;
    case MorphologyShape::Ellipse:
        return cv::MORPH_ELLIPSE;
    case MorphologyShape::Rect:
    default:
        return cv::MORPH_RECT;
    }
}

} // namespace

cv::Mat applyMorphology(const cv::Mat &image, const MorphologyParams &params)
{
    if (image.empty())
        return {};

    const cv::Mat kernel = cv::getStructuringElement(
        toShape(params.shape),
        cv::Size(std::max(1, params.kernelWidth),
                 std::max(1, params.kernelHeight)));

    // Handle BGR separately so the alpha channel is not eroded/dilated
    cv::Mat work = image;
    cv::Mat alpha;
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]},
                  work);
    }

    cv::Mat result;
    const cv::Scalar borderValue = cv::Scalar::all(params.borderValue);
    try {
        switch (params.operation) {
        case MorphologyOperation::Erode:
            cv::erode(work, result, kernel, params.anchor, params.iterations,
                      params.borderType, borderValue);
            break;
        case MorphologyOperation::Open:
        case MorphologyOperation::Close: {
            const bool opening =
                params.operation == MorphologyOperation::Open;
            if (params.method == MorphologyMethod::MorphologyEx) {
                cv::morphologyEx(work, result,
                                 opening ? cv::MORPH_OPEN : cv::MORPH_CLOSE,
                                 kernel, params.anchor, params.iterations,
                                 params.borderType, borderValue);
            } else {
                // Collaboration of dilation and erosion: open = erode then
                // dilate, close = dilate then erode
                cv::Mat intermediate;
                if (opening) {
                    cv::erode(work, intermediate, kernel, params.anchor,
                              params.iterations, params.borderType,
                              borderValue);
                    cv::dilate(intermediate, result, kernel, params.anchor,
                               params.iterations, params.borderType,
                               borderValue);
                } else {
                    cv::dilate(work, intermediate, kernel, params.anchor,
                               params.iterations, params.borderType,
                               borderValue);
                    cv::erode(intermediate, result, kernel, params.anchor,
                              params.iterations, params.borderType,
                              borderValue);
                }
            }
            break;
        }
        case MorphologyOperation::BasicGradient: {
            cv::Mat dilated;
            cv::Mat eroded;
            cv::dilate(work, dilated, kernel, params.anchor,
                       params.iterations, params.borderType, borderValue);
            cv::erode(work, eroded, kernel, params.anchor, params.iterations,
                      params.borderType, borderValue);
            cv::subtract(dilated, eroded, result);
            break;
        }
        case MorphologyOperation::InternalGradient: {
            cv::Mat eroded;
            cv::erode(work, eroded, kernel, params.anchor, params.iterations,
                      params.borderType, borderValue);
            cv::subtract(work, eroded, result);
            break;
        }
        case MorphologyOperation::ExternalGradient: {
            cv::Mat dilated;
            cv::dilate(work, dilated, kernel, params.anchor,
                       params.iterations, params.borderType, borderValue);
            cv::subtract(dilated, work, result);
            break;
        }
        case MorphologyOperation::Dilate:
        default:
            cv::dilate(work, result, kernel, params.anchor, params.iterations,
                       params.borderType, borderValue);
            break;
        }
    } catch (const cv::Exception &) {
        return {};
    }

    if (!alpha.empty()) {
        std::vector<cv::Mat> channels;
        cv::split(result, channels);
        channels.push_back(alpha);
        cv::merge(channels, result);
    }
    return result;
}

} // namespace chameleon

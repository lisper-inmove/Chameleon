#pragma once

#include <vector>

#include <opencv2/core.hpp>

namespace chameleon {

// Binary-image processing (OpenCV-only, Qt-free). First tool:
// threshold segmentation via cv::threshold.

// How the threshold is chosen/applied.
enum class ThresholdMethod {
    Fixed,     // manual threshold value
    Otsu,      // cv::THRESH_OTSU (computed threshold is reported back)
    Triangle,  // cv::THRESH_TRIANGLE (computed threshold is reported back)
    Adaptive,  // cv::adaptiveThreshold (local, per-neighborhood threshold)
};

enum class AdaptiveThresholdMethod {
    Mean,      // cv::ADAPTIVE_THRESH_MEAN_C
    Gaussian,  // cv::ADAPTIVE_THRESH_GAUSSIAN_C
};

enum class ThresholdType {
    Binary,     // > thresh: maxval, else 0
    BinaryInv,  // > thresh: 0, else maxval
    Trunc,      // > thresh: thresh, else unchanged
    ToZero,     // > thresh: unchanged, else 0
    ToZeroInv,  // > thresh: 0, else unchanged
};

struct ThresholdParams {
    ThresholdMethod method = ThresholdMethod::Fixed;
    double thresh = 128.0;  // manual threshold value (Fixed)
    double maxval = 255.0;  // value assigned by the binary variants
    ThresholdType type = ThresholdType::Binary;
    // Adaptive (cv::adaptiveThreshold); only Binary/BinaryInv apply
    AdaptiveThresholdMethod adaptiveMethod = AdaptiveThresholdMethod::Mean;
    int blockSize = 11;  // neighborhood size, odd >= 3
    double c = 2.0;      // constant subtracted from the local mean
};

// Applies the selected thresholding. Fixed thresholds color input per
// channel; the automatic methods (Otsu/Triangle/Adaptive) need a single
// channel, so color input goes through the luminance and the result is
// replicated back to the input's channel count. An alpha channel is
// preserved. For Otsu/Triangle, *computedThresh receives the threshold
// cv::threshold determined; it receives params.thresh otherwise.
cv::Mat applyThreshold(const cv::Mat &image, const ThresholdParams &params,
                       double *computedThresh);

inline cv::Mat applyThreshold(const cv::Mat &image, const ThresholdParams &params)
{
    return applyThreshold(image, params, nullptr);
}

// Connected components labeling.
struct ConnectedComponentsParams {
    int connectivity = 8;   // 4 or 8
    bool withStats = false; // false: cv::connectedComponents, true: WithStats
};

struct ConnectedComponentsResult {
    int count = 0;          // labels incl. background (label 0)
    cv::Mat labels;         // CV_32S label map
    // Per-label statistics, filled only when params.withStats is set
    std::vector<cv::Rect> boundingBoxes;
    std::vector<int> areas;
    std::vector<cv::Point2d> centroids;
};

// Labels the connected regions via cv::connectedComponents (withStats
// false) or cv::connectedComponentsWithStats (true). Multi-channel input
// is converted to its luminance first.
ConnectedComponentsResult connectedComponents(const cv::Mat &image,
                                              const ConnectedComponentsParams &params);

// Visualization: a distinct color per label (golden-ratio hue stepping),
// background black. With annotateIds the label number is drawn at each
// component's centroid (white glyphs with a dark outline). Returns an
// empty Mat for empty input.
cv::Mat colorizeLabels(const cv::Mat &labels, bool annotateIds = false);

} // namespace chameleon

#pragma once

#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/geometry/2d.hpp>  // distance types (fitLine); contour helpers in OpenCV 5
#include <opencv2/imgproc.hpp>  // contour retrieval/approximation and line-type constants

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

// Contour detection (cv::findContours) with drawing (cv::drawContours).
struct ContourParams {
    // Binary input preparation: luminance -> threshold -> optional inversion
    bool otsu = true;       // cv::THRESH_OTSU; the manual threshold is ignored
    int threshold = 128;    // manual threshold value (0..255)
    bool invert = false;    // swap foreground/background after thresholding
    // cv::findContours
    int retrieval = cv::RETR_EXTERNAL;
    int approximation = cv::CHAIN_APPROX_SIMPLE;
    cv::Point offset{0, 0};
    // cv::drawContours
    int thickness = 2;
    int lineType = cv::LINE_AA;
    bool largestOnly = false;  // draw only the largest-area contour
};

// Per-contour measurements, one entry per contour in discovery order.
struct ContourInfo {
    int index = 0;        // 1-based contour number (also drawn on the image)
    double area = 0.0;    // cv::contourArea
    double perimeter = 0.0;  // cv::arcLength(contour, true)
    cv::Rect boundingBox;    // cv::boundingRect
};

struct ContourResult {
    int count = 0;
    std::vector<std::vector<cv::Point>> contours;
    std::vector<ContourInfo> infos;
    // Input image with the contours drawn on top (same size and type);
    // each contour carries its number near the bounding box corner
    cv::Mat image;
};

// Converts to luminance, binarizes, finds contours and draws them in
// green (white on single-channel input). Supports color and alpha input;
// an alpha channel is preserved.
ContourResult findAndDrawContours(const cv::Mat &image,
                                  const ContourParams &params);

// Contour fitting: cv::fitEllipse / cv::fitLine / cv::approxPolyDP.
enum class ContourFitType { Ellipse, Line, Polygon };

struct ContourFitParams {
    ContourFitType type = ContourFitType::Ellipse;
    // cv::fitLine
    int distType = cv::DIST_L2;
    double param = 0.0;  // C: numerical parameter of some distance types
    double reps = 0.01;  // radial accuracy
    double aeps = 0.01;  // angular accuracy
    // cv::approxPolyDP
    double epsilon = 2.0;  // max distance between curve and approximation
    bool closed = true;
    // Drawing of the fitted shapes
    int thickness = 2;
};

// One entry per contour (same numbering as ContourInfo).
struct ContourFitInfo {
    int index = 0;
    bool valid = false;  // false when the contour has too few points
    // Ellipse: center, axes and angle
    cv::RotatedRect ellipse;
    // Line: a point on the fitted line and its unit direction
    cv::Point2f linePoint{0.0f, 0.0f};
    cv::Point2f lineDirection{0.0f, 0.0f};
    // Polygon: approximated vertex count and area
    int vertexCount = 0;
    double polygonArea = 0.0;
};

struct ContourFitResult {
    std::vector<ContourInfo> contourInfos;
    std::vector<ContourFitInfo> fits;
    // Input with all contours (green) and fitted shapes (red) drawn
    cv::Mat image;
};

// Detects contours like findAndDrawContours, then fits every contour with
// the selected method and draws the fitted shapes. Every contour is
// outlined and numbered regardless of ContourParams::largestOnly.
ContourFitResult findAndFitContours(const cv::Mat &image,
                                    const ContourParams &contourParams,
                                    const ContourFitParams &fitParams);

} // namespace chameleon

#pragma once

#include <array>
#include <vector>

#include <opencv2/core.hpp>

namespace chameleon {

// 256-bin histogram (0..255) of one channel of the image, computed with
// OpenCV. Channel indexes are 0-based (B=0, G=1, R=2 for BGR mats).
// Returns all zeros for an empty image or an out-of-range channel.
std::array<double, 256> computeHistogram(const cv::Mat &image, int channel);

// Global histogram equalization (cv::equalizeHist). Single-channel input
// is equalized directly; color input goes through the Y channel of YCrCb
// so the color balance is preserved; an alpha channel is kept untouched.
cv::Mat equalizeHistogram(const cv::Mat &image);

// Contrast-limited adaptive histogram equalization (cv::createCLAHE) with
// the same channel handling as equalizeHistogram().
cv::Mat equalizeClahe(const cv::Mat &image, double clipLimit = 2.0,
                      int tileGridSize = 8);

// Comparison methods for cv::compareHist.
enum class HistogramCompareMethod {
    Correlation,    // HISTCMP_CORREL:        1.0 = identical, larger = more similar
    ChiSquare,      // HISTCMP_CHISQR:        0.0 = identical, smaller = more similar
    Intersect,      // HISTCMP_INTERSECT:     sum of bin minimums, larger = more similar
    Bhattacharyya,  // HISTCMP_BHATTACHARYYA: 0.0 = identical, smaller = more similar
    KlDivergence,   // HISTCMP_KL_DIV:        0.0 = identical, smaller = more similar
};

// Compares the per-channel histograms with cv::compareHist. The
// histograms are L1-normalized (probability distributions) first, so the
// scores do not depend on the image size. Both images are compared over
// min(channelsA, channelsB) channels (gray-vs-color compares the single
// gray channel against the first color channel). Returns one raw score
// per compared channel; empty when either image is empty.
std::vector<double> compareHistograms(const cv::Mat &a, const cv::Mat &b,
                                      HistogramCompareMethod method);

// Maps a raw compareHist score to a normalized similarity in [0, 1] where
// 1.0 means identical distributions. The mapping is uniform across all
// methods so results are directly comparable:
//   Correlation    -> (s + 1) / 2
//   ChiSquare      -> 1 / (1 + max(0, s))
//   Intersect      -> s (already [0, 1] for L1-normalized histograms)
//   Bhattacharyya  -> 1 - s
//   KlDivergence   -> 1 / (1 + max(0, s))
double normalizedSimilarity(HistogramCompareMethod method, double score);

// Histogram matching (specification): transforms the image so its
// histogram matches the template's, via the classic CDF mapping applied
// with cv::LUT. Single-channel input is matched directly; color input
// goes through the Y channel of YCrCb (the template's luminance is the
// target) so the color balance is preserved; alpha is kept untouched.
cv::Mat matchHistogram(const cv::Mat &image, const cv::Mat &templateImage);

// Histogram back projection (cv::calcBackProject): the template's
// normalized histogram is passed as the model (4th parameter) and every
// pixel of the image is mapped to its probability of belonging to the
// template's distribution. Gray input projects its gray channel; color
// input projects the luminance (Y of YCrCb). Returns a CV_8U heat map of
// the same size as the image (255 = strong match); empty on empty inputs.
cv::Mat backProjectHistogram(const cv::Mat &image, const cv::Mat &templateImage);

} // namespace chameleon

#include "processing/histogram.h"

#include <algorithm>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace chameleon {

namespace {

// Runs a single-channel equalizer on the luminance while preserving the
// color balance (Y channel of YCrCb) and any alpha channel
template <typename Equalizer>
cv::Mat equalizeChannels(const cv::Mat &image, Equalizer equalize)
{
    if (image.empty())
        return {};

    if (image.channels() == 1)
        return equalize(image);

    cv::Mat color = image;
    cv::Mat alpha;
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    } else if (image.channels() != 3) {
        return image.clone();
    }

    cv::Mat ycrcb;
    cv::cvtColor(color, ycrcb, cv::COLOR_BGR2YCrCb);
    std::vector<cv::Mat> channels;
    cv::split(ycrcb, channels);
    channels[0] = equalize(channels[0]);
    cv::merge(channels, ycrcb);

    cv::Mat out;
    cv::cvtColor(ycrcb, out, cv::COLOR_YCrCb2BGR);
    if (!alpha.empty()) {
        cv::split(out, channels);
        channels.push_back(alpha);
        cv::merge(channels, out);
    }
    return out;
}

} // namespace

std::array<double, 256> computeHistogram(const cv::Mat &image, int channel)
{
    std::array<double, 256> bins{};
    if (image.empty() || channel < 0 || channel >= image.channels())
        return bins;

    const int channels[] = {channel};
    int histSize[] = {256};
    float range[] = {0, 256};
    const float *ranges[] = {range};
    cv::Mat hist;
    cv::calcHist(&image, 1, channels, cv::Mat(), hist, 1, histSize, ranges, true);
    for (int i = 0; i < 256; ++i)
        bins[i] = hist.at<float>(i);
    return bins;
}

cv::Mat equalizeHistogram(const cv::Mat &image)
{
    return equalizeChannels(image, [](const cv::Mat &singleChannel) {
        cv::Mat out;
        cv::equalizeHist(singleChannel, out);
        return out;
    });
}

cv::Mat equalizeClahe(const cv::Mat &image, double clipLimit, int tileGridSize)
{
    return equalizeChannels(image, [clipLimit, tileGridSize](const cv::Mat &singleChannel) {
        cv::Ptr<cv::CLAHE> clahe =
            cv::createCLAHE(clipLimit, cv::Size(tileGridSize, tileGridSize));
        cv::Mat out;
        clahe->apply(singleChannel, out);
        return out;
    });
}

namespace {

int toCvCompareMethod(HistogramCompareMethod method)
{
    switch (method) {
    case HistogramCompareMethod::ChiSquare:
        return cv::HISTCMP_CHISQR;
    case HistogramCompareMethod::Intersect:
        return cv::HISTCMP_INTERSECT;
    case HistogramCompareMethod::Bhattacharyya:
        return cv::HISTCMP_BHATTACHARYYA;
    case HistogramCompareMethod::KlDivergence:
        return cv::HISTCMP_KL_DIV;
    case HistogramCompareMethod::Correlation:
    default:
        return cv::HISTCMP_CORREL;
    }
}

cv::Mat matchSingleChannel(const cv::Mat &source, const cv::Mat &reference)
{
    const int histSize = 256;
    float range[] = {0, 256};
    const float *ranges[] = {range};
    cv::Mat histSource, histReference;
    cv::calcHist(&source, 1, nullptr, cv::Mat(), histSource, 1, &histSize, ranges, true);
    cv::calcHist(&reference, 1, nullptr, cv::Mat(), histReference, 1, &histSize, ranges, true);

    // Normalized CDFs
    cv::Mat cdfSource = histSource.clone();
    cv::Mat cdfReference = histReference.clone();
    for (int i = 1; i < 256; ++i) {
        cdfSource.at<float>(i) += cdfSource.at<float>(i - 1);
        cdfReference.at<float>(i) += cdfReference.at<float>(i - 1);
    }
    const double sourceTotal = double(source.total()) > 0 ? double(source.total()) : 1.0;
    const double referenceTotal =
        double(reference.total()) > 0 ? double(reference.total()) : 1.0;
    cdfSource = cdfSource / sourceTotal;
    cdfReference = cdfReference / referenceTotal;

    // Classic specification mapping: source level -> smallest template
    // level whose CDF reaches the source CDF (both CDFs are monotonic,
    // so the template cursor only ever moves forward)
    cv::Mat lut(1, 256, CV_8U);
    int j = 0;
    for (int i = 0; i < 256; ++i) {
        while (j < 255 && cdfReference.at<float>(j) < cdfSource.at<float>(i))
            ++j;
        lut.at<uchar>(i) = uchar(j);
    }
    cv::Mat out;
    cv::LUT(source, lut, out);
    return out;
}

cv::Mat luminanceOf(const cv::Mat &image)
{
    if (image.channels() == 1)
        return image.clone();
    cv::Mat color = image;
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    }
    cv::Mat ycrcb;
    cv::cvtColor(color, ycrcb, cv::COLOR_BGR2YCrCb);
    std::vector<cv::Mat> channels;
    cv::split(ycrcb, channels);
    return channels[0];
}

cv::Mat binsToMat(const std::array<double, 256> &bins)
{
    // L1 normalization: probability distributions, so the comparison
    // scores are independent of the image size
    double total = 0.0;
    for (double v : bins)
        total += v;
    const double scale = total > 0.0 ? 1.0 / total : 0.0;
    cv::Mat hist(1, 256, CV_32F);
    for (int i = 0; i < 256; ++i)
        hist.at<float>(0, i) = float(bins[i] * scale);
    return hist;
}

} // namespace

std::vector<double> compareHistograms(const cv::Mat &a, const cv::Mat &b,
                                      HistogramCompareMethod method)
{
    std::vector<double> scores;
    if (a.empty() || b.empty())
        return scores;

    const int channels = std::min(a.channels(), b.channels());
    const int cvMethod = toCvCompareMethod(method);
    for (int channel = 0; channel < channels; ++channel) {
        const cv::Mat histA = binsToMat(computeHistogram(a, channel));
        const cv::Mat histB = binsToMat(computeHistogram(b, channel));
        scores.push_back(cv::compareHist(histA, histB, cvMethod));
    }
    return scores;
}

cv::Mat matchHistogram(const cv::Mat &image, const cv::Mat &templateImage)
{
    if (image.empty() || templateImage.empty())
        return {};

    const cv::Mat templateLuminance = luminanceOf(templateImage);

    if (image.channels() == 1)
        return matchSingleChannel(image, templateLuminance);

    if (image.channels() != 3 && image.channels() != 4)
        return image.clone();

    cv::Mat color = image;
    cv::Mat alpha;
    if (image.channels() == 4) {
        std::vector<cv::Mat> channels;
        cv::split(image, channels);
        alpha = channels[3];
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
    }

    cv::Mat ycrcb;
    cv::cvtColor(color, ycrcb, cv::COLOR_BGR2YCrCb);
    std::vector<cv::Mat> channels;
    cv::split(ycrcb, channels);
    channels[0] = matchSingleChannel(channels[0], templateLuminance);
    cv::merge(channels, ycrcb);

    cv::Mat out;
    cv::cvtColor(ycrcb, out, cv::COLOR_YCrCb2BGR);
    if (!alpha.empty()) {
        cv::split(out, channels);
        channels.push_back(alpha);
        cv::merge(channels, out);
    }
    return out;
}

double normalizedSimilarity(HistogramCompareMethod method, double score)
{
    double similarity = 0.0;
    switch (method) {
    case HistogramCompareMethod::Correlation:
        similarity = (score + 1.0) / 2.0;
        break;
    case HistogramCompareMethod::Intersect:
        similarity = score;  // already [0, 1] after L1 normalization
        break;
    case HistogramCompareMethod::Bhattacharyya:
        similarity = 1.0 - score;
        break;
    case HistogramCompareMethod::ChiSquare:
    case HistogramCompareMethod::KlDivergence:
    default:
        similarity = 1.0 / (1.0 + std::max(0.0, score));
        break;
    }
    return std::clamp(similarity, 0.0, 1.0);
}

} // namespace chameleon

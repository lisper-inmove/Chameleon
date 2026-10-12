#pragma once

#include <opencv2/core.hpp>

#include <string>

namespace chameleon {

// OpenCV-only image file I/O with UTF-8 path support. On Windows
// cv::imread/imwrite take narrow paths and fail on non-ASCII file names,
// so the file bytes are read/written via std::ifstream/std::ofstream
// (using std::filesystem::u8path) and decoded/encoded with cv::imdecode
// and cv::imencode.
namespace ImageIO {

// Returns an empty Mat when the file cannot be read or decoded
cv::Mat imreadUtf8(const std::string &utf8Path);

// Encodes using the path's extension (defaults to PNG); false on failure
bool imwriteUtf8(const std::string &utf8Path, const cv::Mat &image);

} // namespace ImageIO

} // namespace chameleon

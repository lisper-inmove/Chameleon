#pragma once

#include <QPixmap>

#include <opencv2/core.hpp>

namespace chameleon {

// Display-boundary conversion: OpenCV image -> QPixmap for painting.
// QImage appears solely as an internal temporary inside the implementation;
// no image data is stored or passed around as a QImage anywhere in the app.
// Depth is normalized to 8-bit (16U/32F inputs are scaled).
QPixmap pixmapFromMat(const cv::Mat &mat);

} // namespace chameleon

#include "ui/image_conversion.h"

#include <QImage>

#include <opencv2/imgproc.hpp>

namespace chameleon {

QPixmap pixmapFromMat(const cv::Mat &mat)
{
    if (mat.empty())
        return {};

    // Uniform 8-bit working copy; 16-bit and float inputs are scaled
    cv::Mat mat8;
    if (mat.depth() == CV_8U)
        mat8 = mat;
    else if (mat.depth() == CV_16U)
        mat.convertTo(mat8, CV_8U, 1.0 / 257.0);
    else if (mat.depth() == CV_32F || mat.depth() == CV_64F)
        mat.convertTo(mat8, CV_8U, 255.0);
    else
        return {};

    // The QImage below is a transient buffer only; the returned object is
    // a QPixmap and no QImage escapes this function
    cv::Mat rgb;
    switch (mat8.channels()) {
    case 3:
        cv::cvtColor(mat8, rgb, cv::COLOR_BGR2RGB);
        return QPixmap::fromImage(
            QImage(rgb.data, rgb.cols, rgb.rows, qsizetype(rgb.step),
                   QImage::Format_RGB888).copy());
    case 4:
        cv::cvtColor(mat8, rgb, cv::COLOR_BGRA2RGBA);
        return QPixmap::fromImage(
            QImage(rgb.data, rgb.cols, rgb.rows, qsizetype(rgb.step),
                   QImage::Format_RGBA8888).copy());
    case 1:
        return QPixmap::fromImage(
            QImage(mat8.data, mat8.cols, mat8.rows, qsizetype(mat8.step),
                   QImage::Format_Grayscale8).copy());
    default:
        return {};
    }
}

} // namespace chameleon

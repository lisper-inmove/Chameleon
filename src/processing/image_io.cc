#include "processing/image_io.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <opencv2/imgcodecs.hpp>

namespace chameleon {

namespace ImageIO {

cv::Mat imreadUtf8(const std::string &utf8Path)
{
    std::ifstream file(std::filesystem::u8path(utf8Path), std::ios::binary);
    if (!file)
        return {};
    const std::vector<uchar> bytes{std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>()};
    if (bytes.empty())
        return {};
    return cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
}

bool imwriteUtf8(const std::string &utf8Path, const cv::Mat &image)
{
    if (image.empty())
        return false;
    const std::filesystem::path path = std::filesystem::u8path(utf8Path);
    const std::string ext = path.extension().string();
    std::vector<uchar> bytes;
    try {
        if (!cv::imencode(ext.empty() ? ".png" : ext, image, bytes))
            return false;
    } catch (const cv::Exception &) {
        return false;
    }
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;
    file.write(reinterpret_cast<const char *>(bytes.data()),
               std::streamsize(bytes.size()));
    return file.good();
}

} // namespace ImageIO

} // namespace chameleon

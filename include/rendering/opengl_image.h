#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>

namespace chameleon {

// OpenGL image rendering (GLFW + GLAD), independent of Qt.

struct OpenGLRenderResult {
    bool success = false;
    std::string error;
    int width = 0;
    int height = 0;
    // Row-major RGBA, top-down (the GL readback is flipped internally)
    std::vector<unsigned char> rgba;
};

// Renders the image aspect-fit into an offscreen framebuffer using a
// hidden GLFW window and a desktop OpenGL core context, then returns the
// pixels. Success is false (with an error) when no context can be made.
OpenGLRenderResult renderImageOffscreen(const cv::Mat &image, int width,
                                        int height);

// Visible GLFW preview window showing the image with OpenGL; it renders
// on its own thread until the user closes it (window X or Esc).
class OpenGLImageWindow
{
public:
    OpenGLImageWindow();
    ~OpenGLImageWindow();
    OpenGLImageWindow(const OpenGLImageWindow &) = delete;
    OpenGLImageWindow &operator=(const OpenGLImageWindow &) = delete;

    // (Re)starts the preview; a running preview is closed first
    void showImage(const cv::Mat &image, const std::string &title);
    void close();
    bool isRunning() const { return m_running; }

private:
    void run(const cv::Mat &image, const std::string &title);

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_running{false};
    mutable std::mutex m_errorMutex;
    std::string m_error;
};

} // namespace chameleon

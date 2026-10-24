#include "rendering/opengl_image.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <opencv2/imgproc.hpp>

#include <glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

namespace chameleon {

namespace {

constexpr const char *kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

constexpr const char *kFragmentShaderSource = R"(#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main()
{
    FragColor = texture(uTexture, vTexCoord);
}
)";

// Full-screen triangle strip: position.xy, texture-coordinate.uv.
// v = 0 is the image's first (top) row, mapped to the top of the screen.
constexpr float kQuad[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
    1.0f,  -1.0f, 1.0f, 1.0f,
    -1.0f, 1.0f,  0.0f, 0.0f,
    1.0f,  1.0f,  1.0f, 0.0f,
};

struct GlScene {
    GLuint program = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint texture = 0;
};

// Converts any supported 8-bit image to a continuous RGB Mat.
cv::Mat toRgb(const cv::Mat &image)
{
    if (image.empty() || image.depth() != CV_8U)
        return {};
    cv::Mat rgb;
    switch (image.channels()) {
    case 1:
        cv::cvtColor(image, rgb, cv::COLOR_GRAY2RGB);
        break;
    case 3:
        cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
        break;
    case 4:
        cv::cvtColor(image, rgb, cv::COLOR_BGRA2RGB);
        break;
    default:
        return {};
    }
    return rgb;
}

void setContextHints(bool visible)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
}

GLuint compileShader(GLenum type, const char *source, std::string &error)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(size_t(std::max(1, length)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = "shader compile failed: " + log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createProgram(std::string &error)
{
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShaderSource,
                                        error);
    if (vertex == 0)
        return 0;
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER,
                                          kFragmentShaderSource, error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(size_t(std::max(1, length)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        error = "shader link failed: " + log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// Byte offset passed to glVertexAttribPointer as a buffer pointer
const void *vertexOffset(std::size_t offset)
{
    // glVertexAttribPointer expects the byte offset as a pointer value
    return reinterpret_cast<const void *>(  // NOLINT(performance-no-int-to-ptr) - GL API contract
        static_cast<std::uintptr_t>(offset));
}

bool createScene(const cv::Mat &rgb, GlScene &scene, std::string &error)
{
    scene.program = createProgram(error);
    if (scene.program == 0)
        return false;

    glGenVertexArrays(1, &scene.vao);
    glGenBuffers(1, &scene.vbo);
    glBindVertexArray(scene.vao);
    glBindBuffer(GL_ARRAY_BUFFER, scene.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          vertexOffset(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          vertexOffset(2 * sizeof(float)));
    glBindVertexArray(0);

    glGenTextures(1, &scene.texture);
    glBindTexture(GL_TEXTURE_2D, scene.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgb.cols, rgb.rows, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, rgb.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(scene.program);
    glUniform1i(glGetUniformLocation(scene.program, "uTexture"), 0);
    glUseProgram(0);
    return true;
}

void destroyScene(GlScene &scene)
{
    if (scene.texture != 0)
        glDeleteTextures(1, &scene.texture);
    if (scene.vbo != 0)
        glDeleteBuffers(1, &scene.vbo);
    if (scene.vao != 0)
        glDeleteVertexArrays(1, &scene.vao);
    if (scene.program != 0)
        glDeleteProgram(scene.program);
    scene = GlScene{};
}

// Clears the whole framebuffer and draws the image aspect-fit
void drawScene(const GlScene &scene, int framebufferWidth,
               int framebufferHeight, int imageWidth, int imageHeight)
{
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const double scale = std::min(double(framebufferWidth) / imageWidth,
                                  double(framebufferHeight) / imageHeight);
    const int width =
        std::max(1, int(std::lround(imageWidth * scale)));
    const int height =
        std::max(1, int(std::lround(imageHeight * scale)));
    glViewport((framebufferWidth - width) / 2,
               (framebufferHeight - height) / 2, width, height);

    glUseProgram(scene.program);
    glBindVertexArray(scene.vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene.texture);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);
}

void logError(const std::string &error)
{
    spdlog::error("OpenGL: {}", error);
}

} // namespace

OpenGLRenderResult renderImageOffscreen(const cv::Mat &image, int width,  // NOLINT(bugprone-easily-swappable-parameters) - width/height pair
                                        int height)
{
    OpenGLRenderResult result;
    result.width = std::max(1, width);
    result.height = std::max(1, height);
    const cv::Mat rgb = toRgb(image);
    if (rgb.empty()) {
        result.error = "unsupported image (needs an 8-bit 1/3/4-channel Mat)";
        return result;
    }

    if (glfwInit() != GLFW_TRUE) {
        result.error = "glfwInit failed";
        return result;
    }
    setContextHints(false);
    GLFWwindow *window =
        glfwCreateWindow(16, 16, "Chameleon offscreen", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        result.error = "could not create an OpenGL 3.3 core context";
        return result;
    }
    glfwMakeContextCurrent(window);
    if (gladLoadGL() == 0) {
        glfwDestroyWindow(window);
        glfwTerminate();
        result.error = "gladLoadGL failed";
        return result;
    }

    GlScene scene;
    std::string error;
    if (!createScene(rgb, scene, error)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        result.error = error;
        return result;
    }

    GLuint framebuffer = 0;
    GLuint colorTexture = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, result.width, result.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        result.error = "framebuffer incomplete";
    } else {
        drawScene(scene, result.width, result.height, rgb.cols, rgb.rows);

        result.rgba.resize(size_t(result.width) * size_t(result.height) * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, result.width, result.height, GL_RGBA,
                     GL_UNSIGNED_BYTE, result.rgba.data());
        // OpenGL's origin is bottom-left; the result is top-down
        for (int y = 0; y < result.height / 2; ++y) {
            unsigned char *top = result.rgba.data()
                    + size_t(y) * size_t(result.width) * 4;
            unsigned char *bottom = result.rgba.data()
                    + size_t(result.height - 1 - y) * size_t(result.width) * 4;
            std::swap_ranges(top, top + size_t(result.width) * 4, bottom);
        }
        result.success = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &colorTexture);
    glDeleteFramebuffers(1, &framebuffer);
    destroyScene(scene);
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}

OpenGLImageWindow::OpenGLImageWindow() = default;

OpenGLImageWindow::~OpenGLImageWindow()
{
    close();
}

void OpenGLImageWindow::showImage(const cv::Mat &image,
                                  const std::string &title)
{
    if (image.empty())
        return;
    close();
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_error.clear();
    }
    m_stop = false;
    m_running = true;
    m_thread = std::thread([this, image, title] {  // NOLINT(bugprone-exception-escape) - all paths guarded
        // Nothing may escape a thread entry point
        try {
            run(image, title);
        } catch (const std::exception &exception) {
            logError(std::string("preview failed: ") + exception.what());
            m_running = false;
        } catch (...) {
            logError("preview failed: unknown error");
            m_running = false;
        }
    });
}

void OpenGLImageWindow::close()
{
    if (!m_thread.joinable())
        return;
    m_stop = true;
    m_thread.join();
    m_running = false;
}

void OpenGLImageWindow::run(const cv::Mat &image, const std::string &title)
{
    const cv::Mat rgb = toRgb(image);
    if (rgb.empty()) {
        logError("unsupported preview image");
        m_running = false;
        return;
    }

    if (glfwInit() != GLFW_TRUE) {
        logError("glfwInit failed");
        m_running = false;
        return;
    }
    setContextHints(true);
    GLFWwindow *window =
        glfwCreateWindow(960, 720, title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        logError("could not create an OpenGL 3.3 core window");
        glfwTerminate();
        m_running = false;
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (gladLoadGL() == 0) {
        logError("gladLoadGL failed");
        glfwDestroyWindow(window);
        glfwTerminate();
        m_running = false;
        return;
    }

    GlScene scene;
    std::string error;
    if (!createScene(rgb, scene, error)) {
        logError(error);
        glfwDestroyWindow(window);
        glfwTerminate();
        m_running = false;
        return;
    }

    while (!glfwWindowShouldClose(window) && !m_stop) {
        glfwPollEvents();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width > 0 && height > 0)
            drawScene(scene, width, height, rgb.cols, rgb.rows);
        glfwSwapBuffers(window);
    }

    destroyScene(scene);
    glfwDestroyWindow(window);
    glfwTerminate();
    m_running = false;
}

} // namespace chameleon

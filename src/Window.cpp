#include "pa3/Window.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace pa3 {
namespace {
void glfwError(int code, const char* text) {
    std::cerr << "GLFW " << code << ": " << text << '\n';
}
void GLAPIENTRY debugMessage(GLenum, GLenum type, GLuint id, GLenum severity,
                            GLsizei, const GLchar* message, const void*) {
    // Never throw through a C driver callback.
    std::cerr << "GL debug [" << id << "] type=" << type
              << " severity=" << severity << ": " << message << '\n';
}
}
GlfwSession::GlfwSession() {
    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) throw std::runtime_error("glfwInit failed");
}
GlfwSession::~GlfwSession() { glfwTerminate(); }
Window::Window() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    handle_.reset(glfwCreateWindow(1280, 800, "PA3 | Lighting laboratory", nullptr, nullptr));
    if (!handle_) throw std::runtime_error("OpenGL 3.3 Core context unavailable");
    glfwMakeContextCurrent(native());
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK)
        throw std::runtime_error(reinterpret_cast<const char*>(glewGetErrorString(result)));
    // Some GLEW versions probe legacy extensions in Core, setting INVALID_ENUM.
    while (glGetError() != GL_NO_ERROR) {}
    if (!GLEW_VERSION_3_3) throw std::runtime_error("OpenGL >= 3.3 required");
    if ((GLEW_VERSION_4_3 || GLEW_KHR_debug) && glDebugMessageCallback) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugMessage, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
        std::cout << "Debug callback enabled\n";
    } else {
        std::cout << "KHR_debug unavailable: using glGetError checks\n";
    }
    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    // Shader encodes linear output to sRGB; do not encode a second time.
    glDisable(GL_FRAMEBUFFER_SRGB);
    std::cout << "OpenGL: " << glGetString(GL_VERSION)
              << "\nRenderer: " << glGetString(GL_RENDERER) << '\n';
    GLint bits = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &bits);
    if (bits == 0) throw std::runtime_error("Default framebuffer has no depth buffer");
    std::cout << "Depth bits: " << bits << '\n';
    checkErrors();
}
bool Window::down(int key) const { return glfwGetKey(native(), key) == GLFW_PRESS; }
bool Window::pressed(int key) {
    const bool current = down(key);
    const bool edge = current && !previous_.at(static_cast<std::size_t>(key));
    previous_.at(static_cast<std::size_t>(key)) = current;
    return edge;
}
std::pair<int, int> Window::framebuffer() const {
    int width = 0, height = 0;
    glfwGetFramebufferSize(native(), &width, &height);
    return {width, height};
}
void Window::checkErrors() const {
    std::string errors;
    for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError())
        errors += " " + std::to_string(e);
    if (!errors.empty()) throw std::runtime_error("OpenGL errors:" + errors);
}
}

#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <array>
#include <memory>
#include <utility>

namespace pa3 {
class GlfwSession {
public:
    GlfwSession();
    ~GlfwSession();
    GlfwSession(const GlfwSession&) = delete;
    GlfwSession& operator=(const GlfwSession&) = delete;
};
struct WindowDeleter {
    void operator()(GLFWwindow* window) const noexcept { glfwDestroyWindow(window); }
};
class Window {
public:
    Window();
    GLFWwindow* native() const noexcept { return handle_.get(); }
    bool down(int key) const;
    bool pressed(int key);
    std::pair<int, int> framebuffer() const;
    void checkErrors() const;
private:
    // Destruction is reversed: destroy window before terminating GLFW.
    GlfwSession session_;
    std::unique_ptr<GLFWwindow, WindowDeleter> handle_;
    std::array<bool, GLFW_KEY_LAST + 1> previous_{};
};
}

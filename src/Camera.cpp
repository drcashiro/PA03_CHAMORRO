#include "pa3/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
namespace pa3 {
void Camera::update(const Window& window, float dt) {
    yaw_ += (static_cast<float>(window.down(GLFW_KEY_RIGHT))-static_cast<float>(window.down(GLFW_KEY_LEFT)))*dt;
    pitch_ += (static_cast<float>(window.down(GLFW_KEY_UP))-static_cast<float>(window.down(GLFW_KEY_DOWN)))*dt;
    distance_ += (static_cast<float>(window.down(GLFW_KEY_S))-static_cast<float>(window.down(GLFW_KEY_W)))*6.f*dt;
    pitch_ = std::clamp(pitch_, .12f, 1.4f);
    distance_ = std::clamp(distance_, 5.f, 28.f);
    yaw_ = std::remainder(yaw_, glm::two_pi<float>());
}
void Camera::preset(int index) {
    yaw_ = index == 0 ? .55f : -.85f;
    pitch_ = index == 0 ? .55f : .8f;
    distance_ = index == 0 ? 15.f : 13.f;
}
glm::vec3 Camera::position() const {
    return target_ + distance_*glm::vec3(std::cos(pitch_)*std::sin(yaw_),std::sin(pitch_),std::cos(pitch_)*std::cos(yaw_));
}
glm::mat4 Camera::view() const { return glm::lookAt(position(), target_, glm::vec3(0,1,0)); }
glm::mat4 Camera::projection(float aspect) const {
    if (aspect <= 0.f) throw std::runtime_error("Invalid camera aspect ratio");
    return glm::perspective(glm::radians(45.f), aspect, .1f, 100.f);
}
}

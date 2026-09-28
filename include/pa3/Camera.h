#pragma once
#include "Window.h"
#include <glm/glm.hpp>
namespace pa3 {
/** Orbit camera in radians, with bounded pitch and radius to avoid singular views. */
class Camera {
public:
    void update(const Window& window, float dt);
    void preset(int index);
    glm::vec3 position() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
private:
    glm::vec3 target_{0.f, .5f, 0.f};
    float yaw_ = .55f, pitch_ = .55f, distance_ = 15.f;
};
}

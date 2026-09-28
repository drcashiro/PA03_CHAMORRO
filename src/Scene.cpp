#include "pa3/Scene.h"
#include "pa3/Primitives.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
namespace pa3 {
glm::mat4 Object::model(float angle) const {
    // Column vectors: scale first, then rotate, then translate. Never use a zero scale.
    glm::mat4 m = glm::translate(glm::mat4(1.f), position);
    if (animated) m = glm::rotate(m, angle, glm::normalize(glm::vec3(.3f,1.f,.2f)));
    return glm::scale(m, scale);
}
Scene::Scene() {
    meshes.reserve(5);
    meshes.emplace_back(primitives::cube());
    meshes.emplace_back(primitives::sphere());
    meshes.emplace_back(primitives::cylinder());
    meshes.emplace_back(primitives::cylinder(true));
    meshes.emplace_back(primitives::torus());
    objects = {
        {"Ground",0,{0,-.15f,0},{11,.3f,8},{{.8f,.8f,.8f},.12f,24,true,3},false},
        {"Cube",0,{-3,1,1.3f},{1.6f,2,1.6f},{{.72f,.10f,.055f},.35f,40,false,1},false},
        {"Sphere",1,{0,1.05f,1.8f},{1.05f,1.05f,1.05f},{{.035f,.26f,.65f},.8f,96,false,1},false},
        {"Cylinder",2,{3,1,1.1f},{.8f,1,.8f},{{.055f,.48f,.20f},.45f,64,false,1},false},
        {"Cone",3,{-1.9f,1.35f,-2},{1,1.35f,1},{{.85f,.46f,.06f},.35f,32,false,1},false},
        {"Torus",4,{1.7f,1.6f,-1.8f},{1.2f,1.2f,1.2f},{{.44f,.07f,.60f},.75f,80,false,1},true}
    };
}
void Scene::reset(Camera& camera) {
    lighting = Lighting{};
    textureEnabled = specularEnabled = pointEnabled = directionalEnabled = animate = true;
    angle = 0.f;
    objects[2].material.albedo = {.035f,.26f,.65f};
    objects[2].material.shininess = 96.f;
    camera.preset(0);
}
void Scene::applyCase(int index, Camera& camera) {
    reset(camera);
    switch (index) {
    case 0: break;
    case 1: lighting.pointPosition = {4,2,-3}; break;
    case 2: objects[2].material.albedo = {.65f,.055f,.12f}; break;
    case 3: objects[2].material.shininess = 8.f; break;
    case 4: textureEnabled = false; break;
    case 5: camera.preset(1); break;
    default: throw std::runtime_error("Case must be in [0,5]");
    }
    std::cout << "Experiment " << index << ": " << status() << '\n';
}
void Scene::input(Window& window, Camera& camera, float dt) {
    for (int i = 0; i <= 5; ++i)
        if (window.pressed(GLFW_KEY_0+i)) applyCase(i, camera);
    if (window.pressed(GLFW_KEY_T)) textureEnabled = !textureEnabled;
    if (window.pressed(GLFW_KEY_B)) specularEnabled = !specularEnabled;
    if (window.pressed(GLFW_KEY_P)) pointEnabled = !pointEnabled;
    if (window.pressed(GLFW_KEY_L)) directionalEnabled = !directionalEnabled;
    if (window.pressed(GLFW_KEY_SPACE)) animate = !animate;
    if (window.pressed(GLFW_KEY_R)) reset(camera);
    const auto axis = [&window](int plus, int minus) {
        return static_cast<float>(window.down(plus))-static_cast<float>(window.down(minus));
    };
    lighting.pointPosition += 3.f*dt*glm::vec3(axis(GLFW_KEY_D,GLFW_KEY_A),axis(GLFW_KEY_E,GLFW_KEY_Q),axis(GLFW_KEY_K,GLFW_KEY_I));
    lighting.pointPosition = glm::clamp(lighting.pointPosition, glm::vec3(-10,.2f,-10), glm::vec3(10,10,10));
    auto& exponent = objects[2].material.shininess;
    if (window.pressed(GLFW_KEY_LEFT_BRACKET)) exponent = std::max(4.f, exponent*.5f);
    if (window.pressed(GLFW_KEY_RIGHT_BRACKET)) exponent = std::min(256.f, exponent*2.f);
    camera.update(window, dt);
}
void Scene::update(float dt) {
    if (animate) angle = std::fmod(angle + dt*.7f, glm::two_pi<float>());
}
std::string Scene::status() const {
    std::ostringstream text;
    text << "PA3 | tex " << textureEnabled << " | spec " << specularEnabled
         << " | dir " << directionalEnabled << " | point " << pointEnabled
         << " (" << lighting.pointPosition.x << ',' << lighting.pointPosition.y << ',' << lighting.pointPosition.z
         << ") | sphere exponent " << objects[2].material.shininess << " | anim " << animate;
    return text.str();
}
}

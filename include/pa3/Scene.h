#pragma once
#include "Mesh.h"
#include "Camera.h"
#include <string>
namespace pa3 {
struct Material {
    glm::vec3 albedo{1.f}; // Linear RGB, not sRGB UI colors.
    float specular = .4f;
    float shininess = 48.f;
    bool textured = false;
    float uvScale = 1.f;
};
struct Object {
    std::string name;
    std::size_t mesh = 0;
    glm::vec3 position{0.f}, scale{1.f};
    Material material;
    bool animated = false;
    glm::mat4 model(float angle) const;
};
struct Lighting {
    glm::vec3 ambient{.10f};
    glm::vec3 direction{-.6f,-1.f,-.4f}; // Direction in which light rays travel.
    glm::vec3 directionalColor{.65f,.72f,.85f};
    glm::vec3 pointPosition{-3.f,4.f,3.f};
    glm::vec3 pointColor{1.f,.68f,.40f};
    glm::vec3 attenuation{1.f,.09f,.032f};
};
class Scene {
public:
    Scene();
    void input(Window& window, Camera& camera, float dt);
    void update(float dt);
    void applyCase(int index, Camera& camera);
    std::string status() const;
    std::vector<Mesh> meshes;
    std::vector<Object> objects;
    Lighting lighting;
    bool textureEnabled = true, specularEnabled = true;
    bool pointEnabled = true, directionalEnabled = true, animate = true;
    float angle = 0.f;
private:
    void reset(Camera& camera);
};
}

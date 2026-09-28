#pragma once
#include "Shader.h"
#include "Texture.h"
#include "Scene.h"
namespace pa3 {
class Renderer {
public:
    Renderer();
    void draw(const Scene& scene, const Camera& camera, int width, int height);
    void capture(const std::string& path, int width, int height) const;
private:
    Shader shader_;
    Texture checker_;
};
}

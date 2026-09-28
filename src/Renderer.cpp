#include "pa3/Renderer.h"
#include "ShaderSources.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <fstream>
namespace pa3 {
Renderer::Renderer() : shader_(sources::vertex, sources::fragment) {}
void Renderer::draw(const Scene& scene, const Camera& camera, int width, int height) {
    if (width <= 0 || height <= 0) return;
    glViewport(0,0,width,height);
    glClearColor(.055f,.072f,.105f,1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader_.use();
    shader_.set("uView", camera.view());
    shader_.set("uProjection", camera.projection(static_cast<float>(width)/static_cast<float>(height)));
    shader_.set("uEye", camera.position());
    shader_.set("uAmbient", scene.lighting.ambient);
    shader_.set("uDirDirection", scene.lighting.direction);
    shader_.set("uDirColor", scene.directionalEnabled ? scene.lighting.directionalColor : glm::vec3(0));
    shader_.set("uPointPosition", scene.lighting.pointPosition);
    shader_.set("uPointColor", scene.pointEnabled ? scene.lighting.pointColor : glm::vec3(0));
    shader_.set("uAttenuation", scene.lighting.attenuation);
    shader_.set("uTexture", 0);
    checker_.bind();
    for (const auto& object : scene.objects) {
        const glm::mat4 model = object.model(scene.angle);
        shader_.set("uModel", model);
        // Non-uniform scale does not preserve normals. Inverse transpose does.
        shader_.set("uNormalMatrix", glm::inverseTranspose(glm::mat3(model)));
        shader_.set("uAlbedo", object.material.albedo);
        shader_.set("uSpecular", scene.specularEnabled ? object.material.specular : 0.f);
        shader_.set("uShininess", object.material.shininess);
        shader_.set("uUseTexture", static_cast<int>(scene.textureEnabled && object.material.textured));
        shader_.set("uUvScale", object.material.uvScale);
        scene.meshes.at(object.mesh).draw();
    }
    glUseProgram(0);
}
void Renderer::capture(const std::string& path, int width, int height) const {
    // Read back before SwapBuffers. PPM stores top-to-bottom RGB; GL is bottom-up.
    std::vector<unsigned char> rgb(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*3);
    GLint pack = 4; glGetIntegerv(GL_PACK_ALIGNMENT, &pack);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    glPixelStorei(GL_PACK_ALIGNMENT, pack);
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot create capture: " + path);
    file << "P6\n" << width << ' ' << height << "\n255\n";
    const auto stride = static_cast<std::size_t>(width)*3;
    for (int y = height-1; y >= 0; --y)
        file.write(reinterpret_cast<const char*>(rgb.data()+static_cast<std::size_t>(y)*stride),
                   static_cast<std::streamsize>(stride));
    if (!file) throw std::runtime_error("Capture write failed: " + path);
}
}

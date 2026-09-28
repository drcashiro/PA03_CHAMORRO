#pragma once
#include "GlResource.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
namespace pa3 {
struct Vertex { glm::vec3 position; glm::vec3 normal; glm::vec2 uv; };
struct MeshData { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
class Mesh {
public:
    explicit Mesh(const MeshData& data);
    void draw() const;
private:
    GlResource vao_{GlResource::Kind::VertexArray};
    GlResource vbo_{GlResource::Kind::Buffer};
    GlResource ebo_{GlResource::Kind::Buffer};
    GLsizei count_ = 0;
};
}

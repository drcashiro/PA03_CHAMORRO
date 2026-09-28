#include "pa3/Mesh.h"
#include <cstddef>
#include <limits>
#include <type_traits>
namespace pa3 {
Mesh::Mesh(const MeshData& data) {
    static_assert(std::is_standard_layout_v<Vertex>);
    static_assert(sizeof(std::uint32_t) == sizeof(GLuint));
    if (data.vertices.empty() || data.indices.empty() || data.indices.size() % 3 != 0 ||
        data.indices.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::runtime_error("Invalid indexed triangle mesh");
    for (auto index : data.indices)
        if (index >= data.vertices.size()) throw std::runtime_error("Mesh index out of range");
    count_ = static_cast<GLsizei>(data.indices.size());
    glBindVertexArray(vao_.get());
    glBindBuffer(GL_ARRAY_BUFFER, vbo_.get());
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.vertices.size() * sizeof(Vertex)),
                 data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_.get());
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                 data.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
void Mesh::draw() const {
    glBindVertexArray(vao_.get());
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
}

#include "pa3/Primitives.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
namespace pa3::primitives {
namespace {
constexpr float pi = glm::pi<float>();
void triangle(MeshData& mesh, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    // Match CCW winding to analytical outward normals. Skip zero-area pole triangles.
    const auto& va = mesh.vertices[a]; const auto& vb = mesh.vertices[b]; const auto& vc = mesh.vertices[c];
    const glm::vec3 cross = glm::cross(vb.position - va.position, vc.position - va.position);
    if (glm::dot(cross, cross) < 1e-14f) return;
    if (glm::dot(cross, va.normal + vb.normal + vc.normal) < 0.f) std::swap(b, c);
    mesh.indices.insert(mesh.indices.end(), {a, b, c});
}
void resolution(unsigned a, unsigned b = 3) {
    if (a < 3 || b < 3 || a > 512 || b > 512)
        throw std::runtime_error("Primitive resolution must be in [3,512]");
}
}
MeshData cube() {
    MeshData m;
    const glm::vec3 normals[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (const auto& n : normals) {
        const glm::vec3 helper = std::abs(n.y) > .5f ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
        const glm::vec3 u = glm::normalize(glm::cross(helper, n));
        const glm::vec3 v = glm::cross(n, u);
        const auto start = static_cast<std::uint32_t>(m.vertices.size());
        for (const glm::vec2 uv : {glm::vec2(0,0), glm::vec2(1,0), glm::vec2(1,1), glm::vec2(0,1)})
            m.vertices.push_back({n*.5f + (uv.x-.5f)*u + (uv.y-.5f)*v, n, uv});
        triangle(m, start, start+1, start+2); triangle(m, start, start+2, start+3);
    }
    return m;
}
MeshData sphere(unsigned slices, unsigned stacks) {
    resolution(slices, stacks); MeshData m;
    for (unsigned j = 0; j <= stacks; ++j) {
        const float v = static_cast<float>(j)/static_cast<float>(stacks);
        for (unsigned i = 0; i <= slices; ++i) {
            const float u = static_cast<float>(i)/static_cast<float>(slices);
            const glm::vec3 n(std::sin(pi*v)*std::cos(2*pi*u), std::cos(pi*v), std::sin(pi*v)*std::sin(2*pi*u));
            m.vertices.push_back({n, n, {u, 1-v}});
        }
    }
    for (unsigned j = 0; j < stacks; ++j) for (unsigned i = 0; i < slices; ++i) {
        const unsigned a = j*(slices+1)+i, b = a+slices+1;
        triangle(m,a,b,a+1); triangle(m,a+1,b,b+1);
    }
    return m;
}
MeshData cylinder(bool cone, unsigned slices) {
    resolution(slices); MeshData m;
    // Radius=1, height=2. Cone lateral normal follows implicit gradient (x, 1/2, z).
    for (unsigned i = 0; i <= slices; ++i) {
        const float u = static_cast<float>(i)/static_cast<float>(slices);
        const float c = std::cos(2*pi*u), s = std::sin(2*pi*u);
        const glm::vec3 normal = glm::normalize(glm::vec3(c, cone ? .5f : 0.f, s));
        m.vertices.push_back({{c,-1,s}, normal, {u,0}});
        m.vertices.push_back({{cone ? 0.f : c,1,cone ? 0.f : s}, normal, {u,1}});
    }
    for (unsigned i = 0; i < slices; ++i) {
        triangle(m,2*i,2*i+2,2*i+1);
        if (!cone) triangle(m,2*i+1,2*i+2,2*i+3);
    }
    // Separate cap vertices preserve hard normal discontinuities at the rim.
    for (int cap = 0; cap < (cone ? 1 : 2); ++cap) {
        const float y = cap == 0 ? -1.f : 1.f;
        const auto center = static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back({{0,y,0},{0,y,0},{.5f,.5f}});
        for (unsigned i = 0; i <= slices; ++i) {
            const float a = 2*pi*static_cast<float>(i)/static_cast<float>(slices);
            const float c = std::cos(a), s = std::sin(a);
            m.vertices.push_back({{c,y,s},{0,y,0},{.5f+.5f*c,.5f+.5f*s}});
        }
        for (unsigned i = 0; i < slices; ++i) triangle(m,center,center+i+1,center+i+2);
    }
    return m;
}
MeshData torus(unsigned rings, unsigned sides) {
    resolution(rings, sides); MeshData m;
    // Major radius 0.8; tube radius 0.28. Seam duplicates carry distinct UVs.
    for (unsigned i = 0; i <= rings; ++i) for (unsigned j = 0; j <= sides; ++j) {
        const float u = static_cast<float>(i)/static_cast<float>(rings), v = static_cast<float>(j)/static_cast<float>(sides);
        const float a = 2*pi*u, b = 2*pi*v;
        const glm::vec3 n(std::cos(a)*std::cos(b),std::sin(b),std::sin(a)*std::cos(b));
        const glm::vec3 p = glm::vec3(.8f*std::cos(a),0,.8f*std::sin(a)) + .28f*n;
        m.vertices.push_back({p,n,{u,v}});
    }
    for (unsigned i = 0; i < rings; ++i) for (unsigned j = 0; j < sides; ++j) {
        const unsigned a = i*(sides+1)+j, b = a+sides+1;
        triangle(m,a,b,a+1); triangle(m,a+1,b,b+1);
    }
    return m;
}
}

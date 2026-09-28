#pragma once
#include <GL/glew.h>
#include <stdexcept>
#include <utility>

namespace pa3 {
/** Unique ownership of a GL name. A current context must outlive this object.
 * No copying; moving transfers ownership and leaves a harmless empty object.
 */
class GlResource {
public:
    enum class Kind { Buffer, VertexArray, Texture, Program, Shader };
    explicit GlResource(Kind kind, GLenum stage = GL_VERTEX_SHADER) : kind_(kind) {
        switch (kind_) {
        case Kind::Buffer: glGenBuffers(1, &id_); break;
        case Kind::VertexArray: glGenVertexArrays(1, &id_); break;
        case Kind::Texture: glGenTextures(1, &id_); break;
        case Kind::Program: id_ = glCreateProgram(); break;
        case Kind::Shader: id_ = glCreateShader(stage); break;
        }
        if (!id_) throw std::runtime_error("Cannot allocate OpenGL resource");
    }
    ~GlResource() { reset(); }
    GlResource(const GlResource&) = delete;
    GlResource& operator=(const GlResource&) = delete;
    GlResource(GlResource&& rhs) noexcept
        : kind_(rhs.kind_), id_(std::exchange(rhs.id_, 0)) {}
    GlResource& operator=(GlResource&& rhs) noexcept {
        if (this != &rhs) {
            reset(); kind_ = rhs.kind_; id_ = std::exchange(rhs.id_, 0);
        }
        return *this;
    }
    GLuint get() const noexcept { return id_; }
private:
    void reset() noexcept {
        if (!id_) return;
        switch (kind_) {
        case Kind::Buffer: glDeleteBuffers(1, &id_); break;
        case Kind::VertexArray: glDeleteVertexArrays(1, &id_); break;
        case Kind::Texture: glDeleteTextures(1, &id_); break;
        case Kind::Program: glDeleteProgram(id_); break;
        case Kind::Shader: glDeleteShader(id_); break;
        }
        id_ = 0;
    }
    Kind kind_;
    GLuint id_ = 0;
};
}

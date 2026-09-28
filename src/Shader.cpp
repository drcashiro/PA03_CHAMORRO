#include "pa3/Shader.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <vector>
namespace pa3 {
namespace {
GlResource compile(GLenum stage, const char* source) {
    GlResource shader(GlResource::Kind::Shader, stage);
    const GLuint id = shader.get();
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    GLint ok = GL_FALSE;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint size = 0; glGetShaderiv(id, GL_INFO_LOG_LENGTH, &size);
        std::vector<char> log(static_cast<std::size_t>(std::max(size, 1)));
        glGetShaderInfoLog(id, size, nullptr, log.data());
        throw std::runtime_error(std::string(stage == GL_VERTEX_SHADER ? "Vertex: " : "Fragment: ") + log.data());
    }
    return shader;
}
}
Shader::Shader(const char* vertex, const char* fragment) {
    auto vs = compile(GL_VERTEX_SHADER, vertex);
    auto fs = compile(GL_FRAGMENT_SHADER, fragment);
    glAttachShader(program_.get(), vs.get());
    glAttachShader(program_.get(), fs.get());
    glLinkProgram(program_.get());
    GLint ok = GL_FALSE;
    glGetProgramiv(program_.get(), GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint size = 0; glGetProgramiv(program_.get(), GL_INFO_LOG_LENGTH, &size);
        std::vector<char> log(static_cast<std::size_t>(std::max(size, 1)));
        glGetProgramInfoLog(program_.get(), size, nullptr, log.data());
        throw std::runtime_error(std::string("Link: ") + log.data());
    }
    glDetachShader(program_.get(), vs.get());
    glDetachShader(program_.get(), fs.get());
}
GLint Shader::location(const std::string& name) {
    const auto found = locations_.find(name);
    if (found != locations_.end()) return found->second;
    const GLint value = glGetUniformLocation(program_.get(), name.c_str());
    if (value < 0) throw std::runtime_error("Missing active uniform: " + name);
    locations_.emplace(name, value);
    return value;
}
void Shader::set(const std::string& n, int v) { glUniform1i(location(n), v); }
void Shader::set(const std::string& n, float v) { glUniform1f(location(n), v); }
void Shader::set(const std::string& n, const glm::vec3& v) { glUniform3fv(location(n), 1, glm::value_ptr(v)); }
void Shader::set(const std::string& n, const glm::mat3& v) { glUniformMatrix3fv(location(n), 1, GL_FALSE, glm::value_ptr(v)); }
void Shader::set(const std::string& n, const glm::mat4& v) { glUniformMatrix4fv(location(n), 1, GL_FALSE, glm::value_ptr(v)); }
}

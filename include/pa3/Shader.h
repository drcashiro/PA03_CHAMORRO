#pragma once
#include "GlResource.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
namespace pa3 {
class Shader {
public:
    Shader(const char* vertex, const char* fragment);
    void use() const { glUseProgram(program_.get()); }
    void set(const std::string& name, int value);
    void set(const std::string& name, float value);
    void set(const std::string& name, const glm::vec3& value);
    void set(const std::string& name, const glm::mat3& value);
    void set(const std::string& name, const glm::mat4& value);
private:
    GLint location(const std::string& name);
    GlResource program_{GlResource::Kind::Program};
    std::unordered_map<std::string, GLint> locations_;
};
}

#pragma once

#include <string>

#include <glm/glm.hpp>

namespace globe {

// Compiles and links a vertex+fragment shader program from two GLSL files.
// Throws std::runtime_error with the compiler/linker log on failure.
class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;
    void setMat4(const char* name, const glm::mat4& value) const;
    void setInt(const char* name, int value) const;

    [[nodiscard]] unsigned int id() const { return program_; }

private:
    unsigned int program_ = 0;
};

} // namespace globe

#include "render/Shader.h"

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace globe {

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open shader file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int compile(unsigned int type, const std::string& src, const std::string& path) {
    unsigned int shader = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(shader, 1, &c, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile failed (" + path + "):\n" + log.data());
    }
    return shader;
}

} // namespace

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    unsigned int vs = compile(GL_VERTEX_SHADER, readFile(vertPath), vertPath);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, readFile(fragPath), fragPath);

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    int ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        int len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1);
        glGetProgramInfoLog(program_, len, nullptr, log.data());
        glDeleteProgram(program_);
        throw std::runtime_error(std::string("Shader link failed:\n") + log.data());
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

Shader::~Shader() {
    if (program_) {
        glDeleteProgram(program_);
    }
}

void Shader::use() const {
    glUseProgram(program_);
}

void Shader::setMat4(const char* name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(program_, name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(glGetUniformLocation(program_, name), value);
}

} // namespace globe

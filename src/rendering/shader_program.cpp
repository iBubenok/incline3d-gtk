/**
 * @file shader_program.cpp
 * @brief Реализация обёртки для шейдерных программ
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "shader_program.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace incline::rendering {

ShaderProgram::ShaderProgram(std::string_view vertex_source, std::string_view fragment_source) {
    compile(vertex_source, fragment_source);
}

ShaderProgram::~ShaderProgram() {
    cleanup();
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_id_(other.program_id_)
    , uniform_cache_(std::move(other.uniform_cache_))
    , attrib_cache_(std::move(other.attrib_cache_))
{
    other.program_id_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        cleanup();
        program_id_ = other.program_id_;
        uniform_cache_ = std::move(other.uniform_cache_);
        attrib_cache_ = std::move(other.attrib_cache_);
        other.program_id_ = 0;
    }
    return *this;
}

void ShaderProgram::compile(std::string_view vertex_source, std::string_view fragment_source) {
    cleanup();

    GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_source);

    try {
        linkProgram(vertex_shader, fragment_shader);
    } catch (...) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        throw;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    uniform_cache_.clear();
    attrib_cache_.clear();
}

GLuint ShaderProgram::compileShader(GLenum type, std::string_view source) {
    GLuint shader = glCreateShader(type);

    const char* src_ptr = source.data();
    GLint src_length = static_cast<GLint>(source.length());
    glShaderSource(shader, 1, &src_ptr, &src_length);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(static_cast<size_t>(log_length) + 1);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());
        glDeleteShader(shader);

        std::string shader_type = (type == GL_VERTEX_SHADER) ? "вершинного" : "фрагментного";
        throw ShaderError("Ошибка компиляции " + shader_type + " шейдера: " + log.data());
    }

    return shader;
}

void ShaderProgram::linkProgram(GLuint vertex_shader, GLuint fragment_shader) {
    program_id_ = glCreateProgram();
    glAttachShader(program_id_, vertex_shader);
    glAttachShader(program_id_, fragment_shader);
    glLinkProgram(program_id_);

    GLint success = 0;
    glGetProgramiv(program_id_, GL_LINK_STATUS, &success);

    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program_id_, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(static_cast<size_t>(log_length) + 1);
        glGetProgramInfoLog(program_id_, log_length, nullptr, log.data());

        glDeleteProgram(program_id_);
        program_id_ = 0;

        throw ShaderError("Ошибка линковки шейдерной программы: " + std::string(log.data()));
    }
}

void ShaderProgram::cleanup() {
    if (program_id_ != 0) {
        glDeleteProgram(program_id_);
        program_id_ = 0;
    }
}

void ShaderProgram::use() const {
    glUseProgram(program_id_);
}

GLint ShaderProgram::getUniformLocation(std::string_view name) {
    std::string name_str(name);
    auto it = uniform_cache_.find(name_str);
    if (it != uniform_cache_.end()) {
        return it->second;
    }

    GLint location = glGetUniformLocation(program_id_, name_str.c_str());
    uniform_cache_[name_str] = location;
    return location;
}

GLint ShaderProgram::getAttribLocation(std::string_view name) {
    std::string name_str(name);
    auto it = attrib_cache_.find(name_str);
    if (it != attrib_cache_.end()) {
        return it->second;
    }

    GLint location = glGetAttribLocation(program_id_, name_str.c_str());
    attrib_cache_[name_str] = location;
    return location;
}

void ShaderProgram::setUniform(std::string_view name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void ShaderProgram::setUniform(std::string_view name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void ShaderProgram::setUniform(std::string_view name, const glm::vec2& value) {
    glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setUniform(std::string_view name, const glm::vec3& value) {
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setUniform(std::string_view name, const glm::vec4& value) {
    glUniform4fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setUniform(std::string_view name, const glm::mat3& value) {
    glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderProgram::setUniform(std::string_view name, const glm::mat4& value) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace incline::rendering

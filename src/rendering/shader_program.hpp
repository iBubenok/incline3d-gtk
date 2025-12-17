/**
 * @file shader_program.hpp
 * @brief Обёртка для OpenGL шейдерных программ
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include <epoxy/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>

namespace incline::rendering {

/**
 * @brief Ошибка компиляции/линковки шейдера
 */
class ShaderError : public std::runtime_error {
public:
    explicit ShaderError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Обёртка для OpenGL шейдерной программы
 *
 * RAII-класс для управления жизненным циклом шейдеров.
 */
class ShaderProgram {
public:
    ShaderProgram() = default;

    /**
     * @brief Создать программу из исходников шейдеров
     * @param vertex_source Исходный код вершинного шейдера
     * @param fragment_source Исходный код фрагментного шейдера
     * @throws ShaderError При ошибке компиляции/линковки
     */
    ShaderProgram(std::string_view vertex_source, std::string_view fragment_source);

    ~ShaderProgram();

    // Запрет копирования
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Разрешаем перемещение
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    /**
     * @brief Компиляция и линковка программы
     */
    void compile(std::string_view vertex_source, std::string_view fragment_source);

    /**
     * @brief Активировать программу для использования
     */
    void use() const;

    /**
     * @brief Проверить, создана ли программа
     */
    [[nodiscard]] bool isValid() const noexcept { return program_id_ != 0; }

    /**
     * @brief Получить идентификатор программы
     */
    [[nodiscard]] GLuint getId() const noexcept { return program_id_; }

    // === Uniform setters ===

    void setUniform(std::string_view name, int value);
    void setUniform(std::string_view name, float value);
    void setUniform(std::string_view name, const glm::vec2& value);
    void setUniform(std::string_view name, const glm::vec3& value);
    void setUniform(std::string_view name, const glm::vec4& value);
    void setUniform(std::string_view name, const glm::mat3& value);
    void setUniform(std::string_view name, const glm::mat4& value);

    /**
     * @brief Получить расположение uniform-переменной
     * @return -1 если не найдена
     */
    [[nodiscard]] GLint getUniformLocation(std::string_view name);

    /**
     * @brief Получить расположение атрибута
     * @return -1 если не найден
     */
    [[nodiscard]] GLint getAttribLocation(std::string_view name);

private:
    GLuint compileShader(GLenum type, std::string_view source);
    void linkProgram(GLuint vertex_shader, GLuint fragment_shader);
    void cleanup();

    GLuint program_id_{0};
    std::unordered_map<std::string, GLint> uniform_cache_;
    std::unordered_map<std::string, GLint> attrib_cache_;
};

// === Встроенные шейдеры ===

/**
 * @brief Простой шейдер для рисования линий одним цветом
 */
namespace shaders {

constexpr const char* SIMPLE_VERTEX = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)glsl";

constexpr const char* SIMPLE_FRAGMENT = R"glsl(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)glsl";

/**
 * @brief Шейдер с вершинными цветами
 */
constexpr const char* VERTEX_COLOR_VERTEX = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMVP;
out vec4 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)glsl";

constexpr const char* VERTEX_COLOR_FRAGMENT = R"glsl(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)glsl";

/**
 * @brief Шейдер для рисования толстых линий
 */
constexpr const char* LINE_VERTEX = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aOffset;
uniform mat4 uMVP;
uniform float uLineWidth;
uniform vec2 uViewportSize;
out vec4 vColor;
void main() {
    vec4 clipPos = uMVP * vec4(aPos, 1.0);
    vec2 offset = aOffset * uLineWidth / uViewportSize * clipPos.w;
    clipPos.xy += offset;
    gl_Position = clipPos;
    vColor = aColor;
}
)glsl";

constexpr const char* LINE_FRAGMENT = R"glsl(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)glsl";

} // namespace shaders

} // namespace incline::rendering

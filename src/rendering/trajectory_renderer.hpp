/**
 * @file trajectory_renderer.hpp
 * @brief Рендеринг траекторий скважин в 3D
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/well_result.hpp"
#include "model/project.hpp"
#include "camera.hpp"
#include "shader_program.hpp"
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace incline::rendering {

using namespace incline::model;

/**
 * @brief Настройки отображения траектории
 */
struct TrajectoryRenderSettings {
    float line_width = 2.0f;           ///< Толщина линии траектории
    bool show_depth_labels = true;      ///< Показывать отметки глубин
    Meters depth_label_interval{100.0}; ///< Интервал отметок глубин
    bool show_points = false;           ///< Показывать точки замеров
    float point_size = 4.0f;            ///< Размер точек
};

/**
 * @brief Настройки сетки
 */
struct GridSettings {
    bool show_horizontal = true;       ///< Горизонтальная сетка
    bool show_vertical = false;        ///< Вертикальные сетки
    bool show_plan = false;            ///< Плановая сетка (на устье)
    Meters grid_interval{100.0};       ///< Шаг сетки
    Meters horizontal_depth{0.0};      ///< Глубина горизонтальной сетки
    Color grid_color{0.8f, 0.8f, 0.8f, 1.0f};
};

/**
 * @brief Настройки отображения осей и дополнительных элементов
 */
struct SceneSettings {
    bool show_axes = true;             ///< Показывать оси координат
    bool show_sea_level = true;        ///< Показывать уровень моря
    Color sea_level_color{0.7f, 0.85f, 1.0f, 0.5f};
    Color background_color{1.0f, 1.0f, 1.0f, 1.0f};
    Color axis_x_color{1.0f, 0.0f, 0.0f, 1.0f};  ///< Ось X (Север) - красный
    Color axis_y_color{0.0f, 1.0f, 0.0f, 1.0f};  ///< Ось Y (Восток) - зелёный
    Color axis_z_color{0.0f, 0.0f, 1.0f, 1.0f};  ///< Ось Z (Вниз) - синий
};

/**
 * @brief Рендерер 3D аксонометрии
 *
 * Отрисовывает траектории скважин, сетки, оси и дополнительные элементы.
 */
class TrajectoryRenderer {
public:
    TrajectoryRenderer();
    ~TrajectoryRenderer();

    // Запрет копирования
    TrajectoryRenderer(const TrajectoryRenderer&) = delete;
    TrajectoryRenderer& operator=(const TrajectoryRenderer&) = delete;

    /**
     * @brief Инициализация OpenGL ресурсов
     *
     * Должна вызываться после создания GL контекста.
     */
    void initialize();

    /**
     * @brief Освобождение ресурсов
     */
    void cleanup();

    /**
     * @brief Установить размер области отрисовки
     */
    void setViewportSize(int width, int height);

    /**
     * @brief Обновить данные траекторий из проекта
     */
    void updateFromProject(const Project& project);

    /**
     * @brief Добавить траекторию скважины
     */
    void addTrajectory(const WellResult& well, const Color& color, bool visible = true);

    /**
     * @brief Очистить все траектории
     */
    void clearTrajectories();

    /**
     * @brief Отрисовка сцены
     */
    void render(const Camera& camera);

    // === Настройки ===

    void setTrajectorySettings(const TrajectoryRenderSettings& settings);
    void setGridSettings(const GridSettings& settings);
    void setSceneSettings(const SceneSettings& settings);

    [[nodiscard]] const TrajectoryRenderSettings& getTrajectorySettings() const { return trajectory_settings_; }
    [[nodiscard]] const GridSettings& getGridSettings() const { return grid_settings_; }
    [[nodiscard]] const SceneSettings& getSceneSettings() const { return scene_settings_; }

    /**
     * @brief Получить границы сцены (AABB)
     */
    [[nodiscard]] std::pair<glm::vec3, glm::vec3> getSceneBounds() const;

    /**
     * @brief Получить центр сцены
     */
    [[nodiscard]] glm::vec3 getSceneCenter() const;

private:
    struct TrajectoryData {
        std::vector<glm::vec3> points;
        Color color;
        bool visible;
        std::string name;
    };

    void buildVertexBuffers();
    void renderTrajectories(const Camera& camera);
    void renderGrid(const Camera& camera);
    void renderAxes(const Camera& camera);
    void renderSeaLevel(const Camera& camera);

    // Shader programs
    std::unique_ptr<ShaderProgram> simple_shader_;
    std::unique_ptr<ShaderProgram> line_shader_;

    // Vertex buffers
    GLuint trajectory_vao_{0};
    GLuint trajectory_vbo_{0};
    GLuint grid_vao_{0};
    GLuint grid_vbo_{0};
    GLuint axes_vao_{0};
    GLuint axes_vbo_{0};

    // Данные
    std::vector<TrajectoryData> trajectories_;
    bool buffers_dirty_{true};

    // Настройки
    TrajectoryRenderSettings trajectory_settings_;
    GridSettings grid_settings_;
    SceneSettings scene_settings_;

    // Viewport
    int viewport_width_{800};
    int viewport_height_{600};

    // Границы сцены
    glm::vec3 scene_min_{0.0f};
    glm::vec3 scene_max_{0.0f};

    bool initialized_{false};
};

} // namespace incline::rendering

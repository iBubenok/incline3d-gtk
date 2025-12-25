/**
 * @file trajectory_renderer.cpp
 * @brief Реализация рендерера траекторий
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "trajectory_renderer.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <limits>

namespace incline::rendering {

namespace {

glm::vec4 toGlColor(const Color& color) {
    return glm::vec4(
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f
    );
}

} // namespace

TrajectoryRenderer::TrajectoryRenderer() = default;

TrajectoryRenderer::~TrajectoryRenderer() {
    cleanup();
}

void TrajectoryRenderer::initialize() {
    if (initialized_) return;

    // Компиляция шейдеров
    simple_shader_ = std::make_unique<ShaderProgram>(
        shaders::SIMPLE_VERTEX,
        shaders::SIMPLE_FRAGMENT
    );

    line_shader_ = std::make_unique<ShaderProgram>(
        shaders::VERTEX_COLOR_VERTEX,
        shaders::VERTEX_COLOR_FRAGMENT
    );

    // Создание VAO/VBO для траекторий
    glGenVertexArrays(1, &trajectory_vao_);
    glGenBuffers(1, &trajectory_vbo_);

    // Создание VAO/VBO для сетки
    glGenVertexArrays(1, &grid_vao_);
    glGenBuffers(1, &grid_vbo_);

    // Создание VAO/VBO для осей
    glGenVertexArrays(1, &axes_vao_);
    glGenBuffers(1, &axes_vbo_);

    initialized_ = true;
}

void TrajectoryRenderer::cleanup() {
    if (!initialized_) return;

    if (trajectory_vao_) glDeleteVertexArrays(1, &trajectory_vao_);
    if (trajectory_vbo_) glDeleteBuffers(1, &trajectory_vbo_);
    if (grid_vao_) glDeleteVertexArrays(1, &grid_vao_);
    if (grid_vbo_) glDeleteBuffers(1, &grid_vbo_);
    if (axes_vao_) glDeleteVertexArrays(1, &axes_vao_);
    if (axes_vbo_) glDeleteBuffers(1, &axes_vbo_);

    trajectory_vao_ = trajectory_vbo_ = 0;
    grid_vao_ = grid_vbo_ = 0;
    axes_vao_ = axes_vbo_ = 0;

    simple_shader_.reset();
    line_shader_.reset();

    initialized_ = false;
}

void TrajectoryRenderer::setViewportSize(int width, int height) {
    viewport_width_ = std::max(1, width);
    viewport_height_ = std::max(1, height);
}

void TrajectoryRenderer::updateFromProject(const Project& project) {
    clearTrajectories();

    for (const auto& entry : project.wells) {
        if (entry.result.has_value() && !entry.result->points.empty()) {
            addTrajectory(*entry.result, entry.color, entry.visible);
        }
    }
}

void TrajectoryRenderer::addTrajectory(const WellResult& well, const Color& color, bool visible) {
    if (well.points.empty()) return;

    TrajectoryData data;
    data.color = color;
    data.visible = visible;
    data.name = well.well;

    data.points.reserve(well.points.size());
    for (const auto& pt : well.points) {
        // Преобразование в координаты OpenGL:
        // X (Север) -> X
        // Y (Восток) -> Y
        // TVD (Вниз) -> -Z (OpenGL Z вверх)
        data.points.emplace_back(
            static_cast<float>(pt.x.value),
            static_cast<float>(pt.y.value),
            static_cast<float>(-pt.tvd.value)  // Инвертируем для отображения
        );
    }

    trajectories_.push_back(std::move(data));
    buffers_dirty_ = true;

    // Обновляем границы сцены
    for (const auto& p : trajectories_.back().points) {
        scene_min_ = glm::min(scene_min_, p);
        scene_max_ = glm::max(scene_max_, p);
    }
}

void TrajectoryRenderer::clearTrajectories() {
    trajectories_.clear();
    buffers_dirty_ = true;
    scene_min_ = glm::vec3(0.0f);
    scene_max_ = glm::vec3(0.0f);
}

void TrajectoryRenderer::setTrajectorySettings(const TrajectoryRenderSettings& settings) {
    trajectory_settings_ = settings;
}

void TrajectoryRenderer::setGridSettings(const GridSettings& settings) {
    grid_settings_ = settings;
    buffers_dirty_ = true;
}

void TrajectoryRenderer::setSceneSettings(const SceneSettings& settings) {
    scene_settings_ = settings;
}

std::pair<glm::vec3, glm::vec3> TrajectoryRenderer::getSceneBounds() const {
    if (trajectories_.empty()) {
        return {{-100.0f, -100.0f, -1000.0f}, {100.0f, 100.0f, 0.0f}};
    }
    return {scene_min_, scene_max_};
}

glm::vec3 TrajectoryRenderer::getSceneCenter() const {
    auto [min_bound, max_bound] = getSceneBounds();
    return (min_bound + max_bound) * 0.5f;
}

void TrajectoryRenderer::buildVertexBuffers() {
    if (!buffers_dirty_ || !initialized_) return;

    // Ничего не делаем, рисуем напрямую
    buffers_dirty_ = false;
}

void TrajectoryRenderer::render(const Camera& camera) {
    if (!initialized_) return;

    buildVertexBuffers();

    // Очистка фона
    auto bg = toGlColor(scene_settings_.background_color);
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Отрисовка элементов
    renderGrid(camera);
    if (scene_settings_.show_axes) {
        renderAxes(camera);
    }
    if (scene_settings_.show_sea_level) {
        renderSeaLevel(camera);
    }
    renderTrajectories(camera);

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

void TrajectoryRenderer::renderTrajectories(const Camera& camera) {
    if (trajectories_.empty() || !simple_shader_) return;

    simple_shader_->use();
    simple_shader_->setUniform("uMVP", camera.getMVPMatrix());

    glLineWidth(trajectory_settings_.line_width);

    for (const auto& traj : trajectories_) {
        if (!traj.visible || traj.points.empty()) continue;

        simple_shader_->setUniform("uColor", toGlColor(traj.color));

        // Создаём временный буфер для траектории
        glBindVertexArray(trajectory_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, trajectory_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(traj.points.size() * sizeof(glm::vec3)),
            traj.points.data(),
            GL_DYNAMIC_DRAW
        );

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(traj.points.size()));

        // Рисуем точки если нужно
        if (trajectory_settings_.show_points) {
            glPointSize(trajectory_settings_.point_size);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(traj.points.size()));
        }
    }

    glBindVertexArray(0);
}

void TrajectoryRenderer::renderGrid(const Camera& camera) {
    if (!grid_settings_.show_horizontal && !grid_settings_.show_vertical) return;
    if (!simple_shader_) return;

    simple_shader_->use();
    simple_shader_->setUniform("uMVP", camera.getMVPMatrix());
    simple_shader_->setUniform("uColor", toGlColor(grid_settings_.grid_color));

    glLineWidth(1.0f);

    auto [min_bound, max_bound] = getSceneBounds();
    float interval = static_cast<float>(grid_settings_.grid_interval.value);

    // Расширяем границы до ближайшего интервала сетки
    float x_min = std::floor(min_bound.x / interval) * interval;
    float x_max = std::ceil(max_bound.x / interval) * interval;
    float y_min = std::floor(min_bound.y / interval) * interval;
    float y_max = std::ceil(max_bound.y / interval) * interval;

    std::vector<glm::vec3> grid_lines;

    if (grid_settings_.show_horizontal) {
        float z = -static_cast<float>(grid_settings_.horizontal_depth.value);

        // Линии вдоль X
        for (float y = y_min; y <= y_max; y += interval) {
            grid_lines.emplace_back(x_min, y, z);
            grid_lines.emplace_back(x_max, y, z);
        }

        // Линии вдоль Y
        for (float x = x_min; x <= x_max; x += interval) {
            grid_lines.emplace_back(x, y_min, z);
            grid_lines.emplace_back(x, y_max, z);
        }
    }

    if (!grid_lines.empty()) {
        glBindVertexArray(grid_vao_);
        glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
        glBufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(grid_lines.size() * sizeof(glm::vec3)),
            grid_lines.data(),
            GL_DYNAMIC_DRAW
        );

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(grid_lines.size()));
        glBindVertexArray(0);
    }
}

void TrajectoryRenderer::renderAxes(const Camera& camera) {
    if (!simple_shader_) return;

    simple_shader_->use();
    simple_shader_->setUniform("uMVP", camera.getMVPMatrix());

    glLineWidth(2.0f);

    auto [min_bound, max_bound] = getSceneBounds();
    float axis_length = std::max({
        max_bound.x - min_bound.x,
        max_bound.y - min_bound.y,
        max_bound.z - min_bound.z
    }) * 0.1f;

    axis_length = std::max(axis_length, 50.0f);

    // Начало осей в центре плана
    glm::vec3 origin(0.0f, 0.0f, 0.0f);

    std::vector<glm::vec3> axis_vertices = {
        // Ось X (Север) - красная
        origin, origin + glm::vec3(axis_length, 0.0f, 0.0f),
        // Ось Y (Восток) - зелёная
        origin, origin + glm::vec3(0.0f, axis_length, 0.0f),
        // Ось Z (Вниз) - синяя
        origin, origin + glm::vec3(0.0f, 0.0f, -axis_length)
    };

    glBindVertexArray(axes_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, axes_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(axis_vertices.size() * sizeof(glm::vec3)),
        axis_vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    // Ось X (красная)
    simple_shader_->setUniform("uColor", toGlColor(scene_settings_.axis_x_color));
    glDrawArrays(GL_LINES, 0, 2);

    // Ось Y (зелёная)
    simple_shader_->setUniform("uColor", toGlColor(scene_settings_.axis_y_color));
    glDrawArrays(GL_LINES, 2, 2);

    // Ось Z (синяя)
    simple_shader_->setUniform("uColor", toGlColor(scene_settings_.axis_z_color));
    glDrawArrays(GL_LINES, 4, 2);

    glBindVertexArray(0);
}

void TrajectoryRenderer::renderSeaLevel(const Camera& camera) {
    if (!simple_shader_) return;

    simple_shader_->use();
    simple_shader_->setUniform("uMVP", camera.getMVPMatrix());
    simple_shader_->setUniform("uColor", toGlColor(scene_settings_.sea_level_color));

    glLineWidth(1.5f);

    auto [min_bound, max_bound] = getSceneBounds();

    // Уровень моря на Z=0 (TVD=0 в нашей системе)
    float z = 0.0f;

    // Расширяем область уровня моря
    float margin = 50.0f;
    float x_min = min_bound.x - margin;
    float x_max = max_bound.x + margin;
    float y_min = min_bound.y - margin;
    float y_max = max_bound.y + margin;

    // Прямоугольник уровня моря
    std::vector<glm::vec3> sea_level_vertices = {
        {x_min, y_min, z}, {x_max, y_min, z},
        {x_max, y_min, z}, {x_max, y_max, z},
        {x_max, y_max, z}, {x_min, y_max, z},
        {x_min, y_max, z}, {x_min, y_min, z}
    };

    glBindVertexArray(grid_vao_);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(sea_level_vertices.size() * sizeof(glm::vec3)),
        sea_level_vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(sea_level_vertices.size()));

    glBindVertexArray(0);
}

} // namespace incline::rendering

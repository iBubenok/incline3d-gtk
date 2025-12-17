/**
 * @file plan_renderer.hpp
 * @brief Рендеринг плана (горизонтальная проекция)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/well_result.hpp"
#include "model/project.hpp"
#include <cairo.h>
#include <vector>
#include <string>

namespace incline::rendering {

using namespace incline::model;

/**
 * @brief Настройки отображения плана
 */
struct PlanRenderSettings {
    float scale = 1.0f;                 ///< Масштаб (пикселей на метр)
    float offset_x = 0.0f;              ///< Смещение X (пиксели)
    float offset_y = 0.0f;              ///< Смещение Y (пиксели)

    bool show_grid = true;              ///< Показывать сетку
    Meters grid_interval{100.0};        ///< Шаг сетки
    bool show_project_points = true;    ///< Показывать проектные точки
    bool show_tolerance_circles = true; ///< Показывать круги допуска
    bool show_well_labels = true;       ///< Показывать подписи скважин
    bool show_north_arrow = true;       ///< Показывать стрелку севера
    bool show_scale_bar = true;         ///< Показывать масштабную линейку

    Color background_color{255, 255, 255, 255};
    Color grid_color{217, 217, 217, 255};
    float trajectory_width = 2.0f;
};

/**
 * @brief Рендерер плана
 *
 * Отрисовывает горизонтальную проекцию траекторий с использованием Cairo.
 */
class PlanRenderer {
public:
    PlanRenderer() = default;

    /**
     * @brief Обновить данные из проекта
     */
    void updateFromProject(const Project& project);

    /**
     * @brief Отрисовка на Cairo surface
     */
    void render(cairo_t* cr, int width, int height);

    /**
     * @brief Установить настройки
     */
    void setSettings(const PlanRenderSettings& settings);

    /**
     * @brief Получить настройки
     */
    [[nodiscard]] const PlanRenderSettings& getSettings() const { return settings_; }

    /**
     * @brief Преобразование мировых координат в экранные
     */
    void worldToScreen(double world_x, double world_y, double& screen_x, double& screen_y) const;

    /**
     * @brief Преобразование экранных координат в мировые
     */
    void screenToWorld(double screen_x, double screen_y, double& world_x, double& world_y) const;

    /**
     * @brief Масштабирование относительно точки
     */
    void zoomAt(double screen_x, double screen_y, float factor);

    /**
     * @brief Панорамирование
     */
    void pan(float dx, float dy);

    /**
     * @brief Сброс вида (подогнать под все траектории)
     */
    void fitToContent(int width, int height);

    /**
     * @brief Получить координаты под курсором
     */
    [[nodiscard]] std::pair<double, double> getWorldCoordinates(double screen_x, double screen_y) const;

private:
    struct TrajectoryData {
        std::vector<std::pair<double, double>> points;  ///< X, Y в метрах
        Color color;
        bool visible;
        std::string name;
    };

    void renderBackground(cairo_t* cr, int width, int height);
    void renderGrid(cairo_t* cr, int width, int height);
    void renderTrajectories(cairo_t* cr);
    void renderProjectPoints(cairo_t* cr);
    void renderLabels(cairo_t* cr);
    void renderNorthArrow(cairo_t* cr, int width, int height);
    void renderScaleBar(cairo_t* cr, int width, int height);

    std::vector<TrajectoryData> trajectories_;
    std::vector<ProjectPoint> project_points_;
    PlanRenderSettings settings_;

    // Границы данных в мировых координатах
    double data_min_x_{0.0}, data_max_x_{0.0};
    double data_min_y_{0.0}, data_max_y_{0.0};

    // Viewport
    int viewport_width_{800};
    int viewport_height_{600};
};

} // namespace incline::rendering

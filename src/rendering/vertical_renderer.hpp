/**
 * @file vertical_renderer.hpp
 * @brief Рендеринг вертикальной проекции
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/well_result.hpp"
#include "model/project.hpp"
#include <cairo.h>
#include <vector>
#include <string>
#include <optional>

namespace incline::rendering {

using namespace incline::model;

/**
 * @brief Настройки отображения вертикальной проекции
 */
struct VerticalRenderSettings {
    float scale_h = 1.0f;               ///< Горизонтальный масштаб
    float scale_v = 1.0f;               ///< Вертикальный масштаб
    float offset_x = 0.0f;              ///< Смещение X (пиксели)
    float offset_y = 0.0f;              ///< Смещение Y (пиксели)

    std::optional<Degrees> projection_azimuth;  ///< Азимут плоскости сечения
    bool auto_azimuth = true;           ///< Автоматический выбор азимута

    bool show_grid = true;              ///< Показывать сетку
    Meters grid_interval_h{100.0};      ///< Шаг горизонтальной сетки
    Meters grid_interval_v{100.0};      ///< Шаг вертикальной сетки
    bool show_sea_level = true;         ///< Показывать уровень моря
    bool show_depth_labels = true;      ///< Показывать отметки глубин
    bool show_well_labels = true;       ///< Показывать подписи скважин
    bool show_header = false;           ///< Показывать шапку

    Color background_color{255, 255, 255, 255};
    Color grid_color{217, 217, 217, 255};
    Color sea_level_color{178, 216, 255, 178};
    float trajectory_width = 2.0f;
};

/**
 * @brief Рендерер вертикальной проекции
 *
 * Отрисовывает проекцию траекторий на вертикальную плоскость.
 * Плоскость может быть задана вручную или выбрана автоматически.
 */
class VerticalRenderer {
public:
    VerticalRenderer() = default;

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
    void setSettings(const VerticalRenderSettings& settings);

    /**
     * @brief Получить настройки
     */
    [[nodiscard]] const VerticalRenderSettings& getSettings() const { return settings_; }

    /**
     * @brief Установить азимут плоскости сечения
     */
    void setProjectionAzimuth(Degrees azimuth);

    /**
     * @brief Включить автоматический выбор азимута
     */
    void setAutoAzimuth();

    /**
     * @brief Получить текущий азимут плоскости
     */
    [[nodiscard]] Degrees getEffectiveAzimuth() const;

    /**
     * @brief Вращение плоскости сечения
     * @param delta_degrees Изменение азимута в градусах
     */
    void rotateProjectionPlane(double delta_degrees);

    /**
     * @brief Масштабирование
     */
    void zoom(float factor);

    /**
     * @brief Панорамирование
     */
    void pan(float dx, float dy);

    /**
     * @brief Сброс вида
     */
    void fitToContent(int width, int height);

    /**
     * @brief Получить координаты под курсором (смещение, глубина)
     */
    [[nodiscard]] std::pair<double, double> getCoordinates(double screen_x, double screen_y) const;

private:
    struct TrajectoryData {
        std::vector<std::pair<double, double>> points;  ///< Смещение вдоль азимута, TVD
        Color color;
        bool visible;
        std::string name;
        double final_shift;  ///< Смещение забоя для автовыбора азимута
        double final_azimuth; ///< Азимут забоя
    };

    void calculateAutoAzimuth();
    void projectTrajectories();
    void worldToScreen(double offset, double tvd, double& screen_x, double& screen_y) const;
    void screenToWorld(double screen_x, double screen_y, double& offset, double& tvd) const;

    void renderBackground(cairo_t* cr, int width, int height);
    void renderGrid(cairo_t* cr, int width, int height);
    void renderSeaLevel(cairo_t* cr, int width, int height);
    void renderTrajectories(cairo_t* cr);
    void renderLabels(cairo_t* cr);
    void renderDepthScale(cairo_t* cr, int width, int height);
    void renderHeader(cairo_t* cr, int width);

    std::vector<TrajectoryData> trajectories_;
    std::vector<TrajectoryData> projected_trajectories_;  ///< Спроецированные данные
    VerticalRenderSettings settings_;

    Degrees auto_azimuth_{0.0};         ///< Вычисленный автоматический азимут

    // Границы спроецированных данных
    double data_min_offset_{0.0}, data_max_offset_{0.0};
    double data_min_tvd_{0.0}, data_max_tvd_{0.0};

    // Viewport
    int viewport_width_{800};
    int viewport_height_{600};

    bool projection_dirty_{true};
};

} // namespace incline::rendering

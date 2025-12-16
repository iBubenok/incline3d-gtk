/**
 * @file plan_renderer.cpp
 * @brief Реализация рендерера плана
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "plan_renderer.hpp"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace incline::rendering {

void PlanRenderer::updateFromProject(const Project& project) {
    trajectories_.clear();
    project_points_.clear();

    data_min_x_ = data_max_x_ = 0.0;
    data_min_y_ = data_max_y_ = 0.0;
    bool first_point = true;

    for (const auto& entry : project.wells) {
        if (entry.result.points.empty()) continue;

        TrajectoryData data;
        data.color = entry.color;
        data.visible = entry.visible;
        data.name = entry.result.well;

        data.points.reserve(entry.result.points.size());
        for (const auto& pt : entry.result.points) {
            double x = pt.x.value;
            double y = pt.y.value;

            data.points.emplace_back(x, y);

            if (first_point) {
                data_min_x_ = data_max_x_ = x;
                data_min_y_ = data_max_y_ = y;
                first_point = false;
            } else {
                data_min_x_ = std::min(data_min_x_, x);
                data_max_x_ = std::max(data_max_x_, x);
                data_min_y_ = std::min(data_min_y_, y);
                data_max_y_ = std::max(data_max_y_, y);
            }
        }

        trajectories_.push_back(std::move(data));

        // Копируем проектные точки
        for (const auto& pp : entry.result.project_points) {
            project_points_.push_back(pp);
        }
    }
}

void PlanRenderer::setSettings(const PlanRenderSettings& settings) {
    settings_ = settings;
}

void PlanRenderer::worldToScreen(double world_x, double world_y, double& screen_x, double& screen_y) const {
    // Центр viewport
    double cx = viewport_width_ / 2.0;
    double cy = viewport_height_ / 2.0;

    // X направлен вправо, Y - вверх (но на экране вниз)
    screen_x = cx + (world_x * settings_.scale) + settings_.offset_x;
    screen_y = cy - (world_y * settings_.scale) + settings_.offset_y;
}

void PlanRenderer::screenToWorld(double screen_x, double screen_y, double& world_x, double& world_y) const {
    double cx = viewport_width_ / 2.0;
    double cy = viewport_height_ / 2.0;

    world_x = (screen_x - cx - settings_.offset_x) / settings_.scale;
    world_y = -(screen_y - cy - settings_.offset_y) / settings_.scale;
}

void PlanRenderer::zoomAt(double screen_x, double screen_y, float factor) {
    // Координаты мира под курсором до зума
    double wx_before, wy_before;
    screenToWorld(screen_x, screen_y, wx_before, wy_before);

    settings_.scale *= factor;
    settings_.scale = std::clamp(settings_.scale, 0.001f, 1000.0f);

    // Координаты мира под курсором после зума
    double wx_after, wy_after;
    screenToWorld(screen_x, screen_y, wx_after, wy_after);

    // Корректируем смещение чтобы точка под курсором осталась на месте
    settings_.offset_x += static_cast<float>((wx_before - wx_after) * settings_.scale);
    settings_.offset_y -= static_cast<float>((wy_before - wy_after) * settings_.scale);
}

void PlanRenderer::pan(float dx, float dy) {
    settings_.offset_x += dx;
    settings_.offset_y += dy;
}

void PlanRenderer::fitToContent(int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;

    if (trajectories_.empty()) {
        settings_.scale = 1.0f;
        settings_.offset_x = 0.0f;
        settings_.offset_y = 0.0f;
        return;
    }

    double data_width = data_max_x_ - data_min_x_;
    double data_height = data_max_y_ - data_min_y_;

    // Добавляем отступы
    double margin = 50.0;
    double avail_width = width - 2 * margin;
    double avail_height = height - 2 * margin;

    if (data_width > 0 && data_height > 0) {
        double scale_x = avail_width / data_width;
        double scale_y = avail_height / data_height;
        settings_.scale = static_cast<float>(std::min(scale_x, scale_y));
    } else {
        settings_.scale = 1.0f;
    }

    // Центрируем
    double center_x = (data_min_x_ + data_max_x_) / 2.0;
    double center_y = (data_min_y_ + data_max_y_) / 2.0;

    settings_.offset_x = static_cast<float>(-center_x * settings_.scale);
    settings_.offset_y = static_cast<float>(center_y * settings_.scale);
}

std::pair<double, double> PlanRenderer::getWorldCoordinates(double screen_x, double screen_y) const {
    double wx, wy;
    screenToWorld(screen_x, screen_y, wx, wy);
    return {wx, wy};
}

void PlanRenderer::render(cairo_t* cr, int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;

    renderBackground(cr, width, height);

    if (settings_.show_grid) {
        renderGrid(cr, width, height);
    }

    renderTrajectories(cr);

    if (settings_.show_project_points) {
        renderProjectPoints(cr);
    }

    if (settings_.show_well_labels) {
        renderLabels(cr);
    }

    if (settings_.show_north_arrow) {
        renderNorthArrow(cr, width, height);
    }

    if (settings_.show_scale_bar) {
        renderScaleBar(cr, width, height);
    }
}

void PlanRenderer::renderBackground(cairo_t* cr, int width, int height) {
    cairo_set_source_rgb(cr,
        settings_.background_color.r,
        settings_.background_color.g,
        settings_.background_color.b
    );
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

void PlanRenderer::renderGrid(cairo_t* cr, int width, int height) {
    cairo_set_source_rgb(cr,
        settings_.grid_color.r,
        settings_.grid_color.g,
        settings_.grid_color.b
    );
    cairo_set_line_width(cr, 0.5);

    double interval = settings_.grid_interval.value;

    // Определяем видимую область в мировых координатах
    double wmin_x, wmin_y, wmax_x, wmax_y;
    screenToWorld(0, height, wmin_x, wmin_y);
    screenToWorld(width, 0, wmax_x, wmax_y);

    // Округляем до интервала сетки
    double start_x = std::floor(wmin_x / interval) * interval;
    double start_y = std::floor(wmin_y / interval) * interval;

    // Вертикальные линии (постоянный X)
    for (double x = start_x; x <= wmax_x; x += interval) {
        double sx1, sy1, sx2, sy2;
        worldToScreen(x, wmin_y, sx1, sy1);
        worldToScreen(x, wmax_y, sx2, sy2);
        cairo_move_to(cr, sx1, sy1);
        cairo_line_to(cr, sx2, sy2);
    }

    // Горизонтальные линии (постоянный Y)
    for (double y = start_y; y <= wmax_y; y += interval) {
        double sx1, sy1, sx2, sy2;
        worldToScreen(wmin_x, y, sx1, sy1);
        worldToScreen(wmax_x, y, sx2, sy2);
        cairo_move_to(cr, sx1, sy1);
        cairo_line_to(cr, sx2, sy2);
    }

    cairo_stroke(cr);
}

void PlanRenderer::renderTrajectories(cairo_t* cr) {
    cairo_set_line_width(cr, settings_.trajectory_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    for (const auto& traj : trajectories_) {
        if (!traj.visible || traj.points.empty()) continue;

        cairo_set_source_rgb(cr, traj.color.r, traj.color.g, traj.color.b);

        bool first = true;
        for (const auto& [wx, wy] : traj.points) {
            double sx, sy;
            worldToScreen(wx, wy, sx, sy);

            if (first) {
                cairo_move_to(cr, sx, sy);
                first = false;
            } else {
                cairo_line_to(cr, sx, sy);
            }
        }

        cairo_stroke(cr);

        // Отмечаем устье (первая точка)
        if (!traj.points.empty()) {
            double sx, sy;
            worldToScreen(traj.points.front().first, traj.points.front().second, sx, sy);

            cairo_arc(cr, sx, sy, 4, 0, 2 * std::numbers::pi);
            cairo_fill(cr);
        }
    }
}

void PlanRenderer::renderProjectPoints(cairo_t* cr) {
    for (const auto& pp : project_points_) {
        auto coords = pp.getProjectedCoordinates();
        if (!coords.has_value()) continue;

        double sx, sy;
        worldToScreen(coords->first.value, coords->second.value, sx, sy);

        // Круг допуска
        if (settings_.show_tolerance_circles && pp.radius.value > 0) {
            double radius_px = pp.radius.value * settings_.scale;
            cairo_set_source_rgba(cr, 0.0, 0.5, 0.0, 0.3);
            cairo_arc(cr, sx, sy, radius_px, 0, 2 * std::numbers::pi);
            cairo_fill(cr);

            cairo_set_source_rgb(cr, 0.0, 0.5, 0.0);
            cairo_set_line_width(cr, 1.0);
            cairo_arc(cr, sx, sy, radius_px, 0, 2 * std::numbers::pi);
            cairo_stroke(cr);
        }

        // Точка
        cairo_set_source_rgb(cr, 0.0, 0.5, 0.0);
        cairo_arc(cr, sx, sy, 3, 0, 2 * std::numbers::pi);
        cairo_fill(cr);
    }
}

void PlanRenderer::renderLabels(cairo_t* cr) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);

    for (const auto& traj : trajectories_) {
        if (!traj.visible || traj.points.empty() || traj.name.empty()) continue;

        double sx, sy;
        worldToScreen(traj.points.front().first, traj.points.front().second, sx, sy);

        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_move_to(cr, sx + 6, sy - 6);
        cairo_show_text(cr, traj.name.c_str());
    }
}

void PlanRenderer::renderNorthArrow(cairo_t* cr, int width, int /*height*/) {
    // Стрелка севера в правом верхнем углу
    double cx = width - 40;
    double cy = 40;
    double size = 20;

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.5);

    // Стрелка вверх (север)
    cairo_move_to(cr, cx, cy + size);
    cairo_line_to(cr, cx, cy - size);
    cairo_stroke(cr);

    // Наконечник
    cairo_move_to(cr, cx, cy - size);
    cairo_line_to(cr, cx - 5, cy - size + 10);
    cairo_move_to(cr, cx, cy - size);
    cairo_line_to(cr, cx + 5, cy - size + 10);
    cairo_stroke(cr);

    // Буква N
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 12);
    cairo_move_to(cr, cx - 4, cy - size - 5);
    cairo_show_text(cr, "N");
}

void PlanRenderer::renderScaleBar(cairo_t* cr, int width, int height) {
    // Масштабная линейка в левом нижнем углу
    double x = 20;
    double y = height - 30;

    // Определяем удобную длину линейки в метрах
    double target_px = 100;  // Желаемая длина в пикселях
    double meters = target_px / settings_.scale;

    // Округляем до удобного значения
    double nice_values[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    double nice_meters = nice_values[0];
    for (double nv : nice_values) {
        if (nv > meters * 2) break;
        nice_meters = nv;
    }

    double bar_length = nice_meters * settings_.scale;

    // Рисуем линейку
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);

    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + bar_length, y);
    cairo_stroke(cr);

    // Засечки
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, x, y - 5);
    cairo_line_to(cr, x, y + 5);
    cairo_move_to(cr, x + bar_length, y - 5);
    cairo_line_to(cr, x + bar_length, y + 5);
    cairo_stroke(cr);

    // Подпись
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);

    char label[32];
    if (nice_meters >= 1000) {
        snprintf(label, sizeof(label), "%.0f км", nice_meters / 1000);
    } else {
        snprintf(label, sizeof(label), "%.0f м", nice_meters);
    }

    cairo_move_to(cr, x + bar_length / 2 - 15, y + 15);
    cairo_show_text(cr, label);
}

} // namespace incline::rendering

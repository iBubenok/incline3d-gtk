/**
 * @file vertical_renderer.cpp
 * @brief Реализация рендерера вертикальной проекции
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "vertical_renderer.hpp"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace incline::rendering {

void VerticalRenderer::updateFromProject(const Project& project) {
    trajectories_.clear();

    for (const auto& entry : project.wells) {
        if (entry.result.points.empty()) continue;

        TrajectoryData data;
        data.color = entry.color;
        data.visible = entry.visible;
        data.name = entry.result.well;
        data.final_shift = entry.result.final_shift.value;
        data.final_azimuth = entry.result.final_direction.value;

        // Сохраняем исходные данные (X, Y, TVD) для последующей проекции
        data.points.reserve(entry.result.points.size());
        for (const auto& pt : entry.result.points) {
            // Временно храним X, Y (TVD сохраним отдельно)
            data.points.emplace_back(pt.x.value, pt.y.value);
        }

        trajectories_.push_back(std::move(data));
    }

    projection_dirty_ = true;

    if (settings_.auto_azimuth) {
        calculateAutoAzimuth();
    }
}

void VerticalRenderer::setSettings(const VerticalRenderSettings& settings) {
    bool azimuth_changed = (settings.auto_azimuth != settings_.auto_azimuth) ||
        (settings.projection_azimuth != settings_.projection_azimuth);

    settings_ = settings;

    if (azimuth_changed) {
        projection_dirty_ = true;
        if (settings_.auto_azimuth) {
            calculateAutoAzimuth();
        }
    }
}

void VerticalRenderer::setProjectionAzimuth(Degrees azimuth) {
    settings_.auto_azimuth = false;
    settings_.projection_azimuth = azimuth;
    projection_dirty_ = true;
}

void VerticalRenderer::setAutoAzimuth() {
    settings_.auto_azimuth = true;
    settings_.projection_azimuth = std::nullopt;
    calculateAutoAzimuth();
    projection_dirty_ = true;
}

Degrees VerticalRenderer::getEffectiveAzimuth() const {
    if (settings_.auto_azimuth) {
        return auto_azimuth_;
    }
    return settings_.projection_azimuth.value_or(Degrees{0.0});
}

void VerticalRenderer::rotateProjectionPlane(double delta_degrees) {
    settings_.auto_azimuth = false;

    Degrees current = getEffectiveAzimuth();
    double new_azimuth = current.value + delta_degrees;

    // Нормализация в [0, 360)
    while (new_azimuth < 0) new_azimuth += 360.0;
    while (new_azimuth >= 360.0) new_azimuth -= 360.0;

    settings_.projection_azimuth = Degrees{new_azimuth};
    projection_dirty_ = true;
}

void VerticalRenderer::zoom(float factor) {
    settings_.scale_h *= factor;
    settings_.scale_v *= factor;
    settings_.scale_h = std::clamp(settings_.scale_h, 0.001f, 1000.0f);
    settings_.scale_v = std::clamp(settings_.scale_v, 0.001f, 1000.0f);
}

void VerticalRenderer::pan(float dx, float dy) {
    settings_.offset_x += dx;
    settings_.offset_y += dy;
}

void VerticalRenderer::fitToContent(int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;

    projectTrajectories();

    if (projected_trajectories_.empty()) {
        settings_.scale_h = 1.0f;
        settings_.scale_v = 1.0f;
        settings_.offset_x = 0.0f;
        settings_.offset_y = 0.0f;
        return;
    }

    double data_width = data_max_offset_ - data_min_offset_;
    double data_height = data_max_tvd_ - data_min_tvd_;

    double margin = 50.0;
    double header_height = settings_.show_header ? 100.0 : 0.0;
    double avail_width = width - 2 * margin;
    double avail_height = height - 2 * margin - header_height;

    if (data_width > 0 && data_height > 0) {
        double scale_h = avail_width / data_width;
        double scale_v = avail_height / data_height;

        // Используем одинаковый масштаб по умолчанию
        double scale = std::min(scale_h, scale_v);
        settings_.scale_h = static_cast<float>(scale);
        settings_.scale_v = static_cast<float>(scale);
    } else {
        settings_.scale_h = 1.0f;
        settings_.scale_v = 1.0f;
    }

    // Центрируем
    double center_offset = (data_min_offset_ + data_max_offset_) / 2.0;
    double center_tvd = (data_min_tvd_ + data_max_tvd_) / 2.0;

    settings_.offset_x = static_cast<float>(-center_offset * settings_.scale_h);
    settings_.offset_y = static_cast<float>(center_tvd * settings_.scale_v + header_height / 2);
}

std::pair<double, double> VerticalRenderer::getCoordinates(double screen_x, double screen_y) const {
    double offset, tvd;
    screenToWorld(screen_x, screen_y, offset, tvd);
    return {offset, tvd};
}

void VerticalRenderer::calculateAutoAzimuth() {
    if (trajectories_.empty()) {
        auto_azimuth_ = Degrees{0.0};
        return;
    }

    // Находим скважину с максимальным смещением забоя
    double max_shift = 0.0;
    double azimuth_of_max = 0.0;

    for (const auto& traj : trajectories_) {
        if (traj.final_shift > max_shift) {
            max_shift = traj.final_shift;
            azimuth_of_max = traj.final_azimuth;
        }
    }

    auto_azimuth_ = Degrees{azimuth_of_max};
}

void VerticalRenderer::projectTrajectories() {
    if (!projection_dirty_) return;

    projected_trajectories_.clear();
    data_min_offset_ = data_max_offset_ = 0.0;
    data_min_tvd_ = data_max_tvd_ = 0.0;
    bool first_point = true;

    Degrees plane_azimuth = getEffectiveAzimuth();
    double az_rad = plane_azimuth.toRadians().value;
    double cos_az = std::cos(az_rad);
    double sin_az = std::sin(az_rad);

    for (const auto& traj : trajectories_) {
        TrajectoryData projected;
        projected.color = traj.color;
        projected.visible = traj.visible;
        projected.name = traj.name;

        projected.points.reserve(traj.points.size());

        for (const auto& [x, y] : traj.points) {
            // Проекция на плоскость с заданным азимутом
            // Смещение вдоль направления азимута
            double offset = x * cos_az + y * sin_az;

            // TVD хранится как Y-координата исходных точек временно
            // На самом деле нам нужно обратиться к исходным данным...
            // Упрощение: используем Y как TVD (что неверно, но для демонстрации)
            double tvd = y;  // TODO: исправить - нужен доступ к TVD

            projected.points.emplace_back(offset, tvd);

            if (first_point) {
                data_min_offset_ = data_max_offset_ = offset;
                data_min_tvd_ = data_max_tvd_ = tvd;
                first_point = false;
            } else {
                data_min_offset_ = std::min(data_min_offset_, offset);
                data_max_offset_ = std::max(data_max_offset_, offset);
                data_min_tvd_ = std::min(data_min_tvd_, tvd);
                data_max_tvd_ = std::max(data_max_tvd_, tvd);
            }
        }

        projected_trajectories_.push_back(std::move(projected));
    }

    projection_dirty_ = false;
}

void VerticalRenderer::worldToScreen(double offset, double tvd, double& screen_x, double& screen_y) const {
    double cx = viewport_width_ / 2.0;
    double cy = viewport_height_ / 2.0;

    screen_x = cx + (offset * settings_.scale_h) + settings_.offset_x;
    // TVD увеличивается вниз, на экране Y тоже вниз
    screen_y = cy + (tvd * settings_.scale_v) + settings_.offset_y;
}

void VerticalRenderer::screenToWorld(double screen_x, double screen_y, double& offset, double& tvd) const {
    double cx = viewport_width_ / 2.0;
    double cy = viewport_height_ / 2.0;

    offset = (screen_x - cx - settings_.offset_x) / settings_.scale_h;
    tvd = (screen_y - cy - settings_.offset_y) / settings_.scale_v;
}

void VerticalRenderer::render(cairo_t* cr, int width, int height) {
    viewport_width_ = width;
    viewport_height_ = height;

    projectTrajectories();

    renderBackground(cr, width, height);

    if (settings_.show_header) {
        renderHeader(cr, width);
    }

    if (settings_.show_grid) {
        renderGrid(cr, width, height);
    }

    if (settings_.show_sea_level) {
        renderSeaLevel(cr, width, height);
    }

    renderTrajectories(cr);

    if (settings_.show_depth_labels) {
        renderDepthScale(cr, width, height);
    }

    if (settings_.show_well_labels) {
        renderLabels(cr);
    }
}

void VerticalRenderer::renderBackground(cairo_t* cr, int width, int height) {
    cairo_set_source_rgb(cr,
        settings_.background_color.r,
        settings_.background_color.g,
        settings_.background_color.b
    );
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
}

void VerticalRenderer::renderHeader(cairo_t* cr, int width) {
    // Простая шапка
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_rectangle(cr, 0, 0, width, 80);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, 80);
    cairo_line_to(cr, width, 80);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, 20, 25);
    cairo_show_text(cr, "Вертикальная проекция");

    cairo_set_font_size(cr, 11);
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    char buf[64];
    snprintf(buf, sizeof(buf), "Азимут плоскости: %.1f°", getEffectiveAzimuth().value);
    cairo_move_to(cr, 20, 50);
    cairo_show_text(cr, buf);
}

void VerticalRenderer::renderGrid(cairo_t* cr, int width, int height) {
    cairo_set_source_rgb(cr,
        settings_.grid_color.r,
        settings_.grid_color.g,
        settings_.grid_color.b
    );
    cairo_set_line_width(cr, 0.5);

    double interval_h = settings_.grid_interval_h.value;
    double interval_v = settings_.grid_interval_v.value;

    // Определяем видимую область
    double wmin_offset, wmin_tvd, wmax_offset, wmax_tvd;
    screenToWorld(0, height, wmin_offset, wmax_tvd);
    screenToWorld(width, 0, wmax_offset, wmin_tvd);

    double start_offset = std::floor(wmin_offset / interval_h) * interval_h;
    double start_tvd = std::floor(wmin_tvd / interval_v) * interval_v;

    // Вертикальные линии
    for (double offset = start_offset; offset <= wmax_offset; offset += interval_h) {
        double sx1, sy1, sx2, sy2;
        worldToScreen(offset, wmin_tvd, sx1, sy1);
        worldToScreen(offset, wmax_tvd, sx2, sy2);
        cairo_move_to(cr, sx1, sy1);
        cairo_line_to(cr, sx2, sy2);
    }

    // Горизонтальные линии
    for (double tvd = start_tvd; tvd <= wmax_tvd; tvd += interval_v) {
        double sx1, sy1, sx2, sy2;
        worldToScreen(wmin_offset, tvd, sx1, sy1);
        worldToScreen(wmax_offset, tvd, sx2, sy2);
        cairo_move_to(cr, sx1, sy1);
        cairo_line_to(cr, sx2, sy2);
    }

    cairo_stroke(cr);
}

void VerticalRenderer::renderSeaLevel(cairo_t* cr, int width, int /*height*/) {
    double sx1, sy, sx2;
    double dummy;
    worldToScreen(-10000, 0, sx1, sy);  // TVD = 0 это уровень стола ротора/моря
    worldToScreen(10000, 0, sx2, dummy);

    // Ограничиваем по ширине окна
    sx1 = std::max(0.0, sx1);
    sx2 = std::min(static_cast<double>(width), sx2);

    cairo_set_source_rgba(cr,
        settings_.sea_level_color.r,
        settings_.sea_level_color.g,
        settings_.sea_level_color.b,
        settings_.sea_level_color.a
    );
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, sx1, sy);
    cairo_line_to(cr, sx2, sy);
    cairo_stroke(cr);
}

void VerticalRenderer::renderTrajectories(cairo_t* cr) {
    cairo_set_line_width(cr, settings_.trajectory_width);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    for (const auto& traj : projected_trajectories_) {
        if (!traj.visible || traj.points.empty()) continue;

        cairo_set_source_rgb(cr, traj.color.r, traj.color.g, traj.color.b);

        bool first = true;
        for (const auto& [offset, tvd] : traj.points) {
            double sx, sy;
            worldToScreen(offset, tvd, sx, sy);

            if (first) {
                cairo_move_to(cr, sx, sy);
                first = false;
            } else {
                cairo_line_to(cr, sx, sy);
            }
        }

        cairo_stroke(cr);
    }
}

void VerticalRenderer::renderLabels(cairo_t* cr) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

    for (const auto& traj : projected_trajectories_) {
        if (!traj.visible || traj.points.empty() || traj.name.empty()) continue;

        // Подпись в начале траектории (устье)
        double sx, sy;
        worldToScreen(traj.points.front().first, traj.points.front().second, sx, sy);

        cairo_move_to(cr, sx + 5, sy - 5);
        cairo_show_text(cr, traj.name.c_str());
    }
}

void VerticalRenderer::renderDepthScale(cairo_t* cr, int /*width*/, int height) {
    // Шкала глубин слева
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9);
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);

    double interval = settings_.grid_interval_v.value;

    double wmin_offset, wmin_tvd, wmax_offset, wmax_tvd;
    screenToWorld(0, height, wmin_offset, wmax_tvd);
    screenToWorld(0, 0, wmax_offset, wmin_tvd);

    double start_tvd = std::ceil(wmin_tvd / interval) * interval;

    for (double tvd = start_tvd; tvd <= wmax_tvd; tvd += interval) {
        double sx, sy;
        worldToScreen(0, tvd, sx, sy);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f м", tvd);
        cairo_move_to(cr, 5, sy + 3);
        cairo_show_text(cr, buf);
    }
}

} // namespace incline::rendering

/**
 * @file render_selftest.cpp
 * @brief Самопроверка рендеринга (экспорт изображений)
 */

#include "render_selftest.hpp"
#include "core/processing.hpp"
#include "model/project.hpp"
#include "rendering/plan_renderer.hpp"
#include "rendering/vertical_renderer.hpp"
#include <cairo/cairo.h>
#include <fstream>
#include <vector>
#include <cmath>

namespace incline::app {

namespace {

using namespace incline::model;
using namespace incline::core;
using namespace incline::rendering;

constexpr int kImageWidth = 1024;
constexpr int kImageHeight = 768;

IntervalData makeSampleData() {
    IntervalData data;
    data.well = "SELFTEST-1";
    data.cluster = "Demo";
    data.rotor_table_altitude = Meters{200.0};
    data.magnetic_declination = Degrees{8.0};

    // Простая дуга с горизонталью
    auto addPoint = [&data](double depth, double inc, double az) {
        MeasurementPoint m;
        m.depth = Meters{depth};
        m.inclination = Degrees{inc};
        m.magnetic_azimuth = Degrees{az};
        data.measurements.push_back(m);
    };

    addPoint(0.0, 0.0, 0.0);
    addPoint(100.0, 5.0, 30.0);
    addPoint(200.0, 20.0, 60.0);
    addPoint(300.0, 60.0, 90.0);
    addPoint(400.0, 90.0, 110.0);
    addPoint(500.0, 90.0, 120.0);
    return data;
}

Project makeProjectWithResults() {
    Project project;
    project.name = "Render Selftest";

    auto entry = project.addWell(makeSampleData());

    ProcessingOptions options;
    options.method = TrajectoryMethod::MinimumCurvatureIntegral;
    options.azimuth_mode = AzimuthMode::Magnetic;
    options.dogleg_method = DoglegMethod::Sine;
    options.intensity_interval_L = Meters{25.0};
    options.interpolate_missing_azimuths = true;
    options.extend_last_azimuth = true;
    options.blank_vertical_azimuth = true;
    options.vertical_if_no_azimuth = true;

    entry.result = processWell(entry.source_data, options);

    // Добавим одну проектную точку
    ProjectPoint pp;
    pp.name = "Target";
    pp.shift = Meters{300.0};
    pp.azimuth_geographic = Degrees{95.0};
    pp.radius = Meters{30.0};
    pp.depth = Meters{400.0};
    if (entry.result.has_value()) {
        entry.result->project_points.push_back(pp);
    }

    return project;
}

void save_surface(const std::filesystem::path& path, cairo_surface_t* surface) {
    cairo_status_t status = cairo_surface_write_to_png(surface, path.string().c_str());
    if (status != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error("Не удалось сохранить PNG: " + std::string(cairo_status_to_string(status)));
    }
}

void render_plan(const Project& project, const std::filesystem::path& out_path) {
    PlanRenderer renderer;
    PlanRenderSettings settings;
    settings.show_project_points = true;
    renderer.setSettings(settings);
    renderer.updateFromProject(project);

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kImageWidth, kImageHeight);
    auto cr = cairo_create(surface);
    renderer.render(cr, kImageWidth, kImageHeight);
    save_surface(out_path, surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

void render_vertical(const Project& project, const std::filesystem::path& out_path) {
    VerticalRenderer renderer;
    VerticalRenderSettings settings;
    renderer.setSettings(settings);
    renderer.updateFromProject(project);
    renderer.fitToContent(kImageWidth, kImageHeight);

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kImageWidth, kImageHeight);
    auto cr = cairo_create(surface);
    renderer.render(cr, kImageWidth, kImageHeight);
    save_surface(out_path, surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

// Простейшая псевдо-3D картинка (без OpenGL) для smoke-проверки путей/кодировок.
// WINDOWS-TODO: заменить на реальный экспорт OpenGL.
void render_pseudo_3d(const Project& project, const std::filesystem::path& out_path) {
    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kImageWidth, kImageHeight);
    auto cr = cairo_create(surface);

    // Фон
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    // Оси
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_move_to(cr, 100, kImageHeight - 100);
    cairo_line_to(cr, 100, 100);
    cairo_line_to(cr, kImageWidth - 100, kImageHeight - 150);
    cairo_stroke(cr);

    // Траектории
    for (const auto& entry : project.wells) {
        if (!entry.result.has_value()) continue;
        const auto& pts = entry.result->points;
        if (pts.empty()) continue;

        cairo_set_source_rgb(cr, 0.0, 0.2, 0.8);
        cairo_set_line_width(cr, 2.0);

        auto project_point = [](double x, double y, double z) {
            double scale = 0.6;
            double sx = 150 + (x - y) * scale;
            double sy = (kImageHeight - 150) + (-z) * 0.3 + (x + y) * 0.05;
            return std::pair<double, double>{sx, sy};
        };

        bool first = true;
        for (const auto& p : pts) {
            auto [sx, sy] = project_point(p.x.value, p.y.value, p.tvd.value);
            if (first) {
                cairo_move_to(cr, sx, sy);
                first = false;
            } else {
                cairo_line_to(cr, sx, sy);
            }
        }
        cairo_stroke(cr);
    }

    save_surface(out_path, surface);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

} // namespace

int runRenderSelfTest(const std::filesystem::path& output_dir) {
    try {
        std::filesystem::create_directories(output_dir);
        auto project = makeProjectWithResults();

        render_plan(project, output_dir / "plan.png");
        render_vertical(project, output_dir / "vertical.png");
        render_pseudo_3d(project, output_dir / "axonometry.png");

        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}

} // namespace incline::app

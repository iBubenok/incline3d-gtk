#if defined(INCLINE3D_HAS_GUI) && INCLINE3D_HAS_GUI

#include <doctest/doctest.h>
#include "rendering/vertical_renderer.hpp"
#include "model/project.hpp"

using namespace incline::rendering;
using namespace incline::model;

TEST_CASE("VerticalRenderer uses TVD for vertical axis") {
    Project project;
    WellEntry entry;
    entry.id = "well-1";
    entry.visible = true;

    WellResult result;
    result.well = "Test well";

    ProcessedPoint p1;
    p1.depth = Meters{0.0};
    p1.x = Meters{0.0};
    p1.y = Meters{0.0};
    p1.tvd = Meters{0.0};

    ProcessedPoint p2;
    p2.depth = Meters{200.0};
    p2.x = Meters{0.0};
    p2.y = Meters{100.0};   // Смещение по Y отличается от TVD, чтобы поймать ошибку
    p2.tvd = Meters{200.0};

    result.points = {p1, p2};
    result.actual_shift = p2.calculatedShift();
    result.actual_direction_angle = Degrees{0.0};

    entry.result = result;
    project.wells.push_back(entry);

    VerticalRenderer renderer;
    VerticalRenderSettings settings;
    settings.auto_azimuth = false;
    settings.projection_azimuth = Degrees{0.0};
    renderer.setSettings(settings);

    renderer.updateFromProject(project);
    renderer.fitToContent(800, 600);

    auto tvd_range = renderer.projectedTvdRange();
    CHECK(tvd_range.first == doctest::Approx(0.0));
    CHECK(tvd_range.second == doctest::Approx(200.0));
}

TEST_CASE("VerticalRenderer projects project points onto chosen plane") {
    Project project;
    WellEntry entry;
    entry.id = "well-2";
    entry.visible = true;

    WellResult result;
    result.well = "Test well 2";

    // Минимальный набор точек траектории
    ProcessedPoint p1;
    p1.depth = Meters{0.0};
    p1.x = Meters{0.0};
    p1.y = Meters{0.0};
    p1.tvd = Meters{0.0};

    ProcessedPoint p2;
    p2.depth = Meters{200.0};
    p2.x = Meters{0.0};
    p2.y = Meters{0.0};
    p2.tvd = Meters{200.0};

    result.points = {p1, p2};
    result.actual_shift = p2.calculatedShift();
    result.actual_direction_angle = Degrees{0.0};

    ProjectPoint pp;
    pp.name = "Target";
    pp.shift = Meters{100.0};
    pp.azimuth_geographic = Degrees{0.0}; // Плановая точка на север (по X)
    pp.radius = Meters{10.0};

    ProjectPointFactual fact;
    fact.x = Meters{110.0};
    fact.y = Meters{0.0};
    fact.tvd = Meters{200.0};
    pp.factual = fact;

    result.project_points.push_back(pp);

    entry.result = result;
    project.wells.push_back(entry);

    VerticalRenderer renderer;
    VerticalRenderSettings settings;
    settings.auto_azimuth = false;
    settings.projection_azimuth = Degrees{0.0};
    renderer.setSettings(settings);

    renderer.updateFromProject(project);
    auto range0 = renderer.projectedOffsetRange();
    CHECK(range0.second == doctest::Approx(110.0)); // 0..110: радиус учитывается

    renderer.setProjectionAzimuth(Degrees{90.0});
    auto range90 = renderer.projectedOffsetRange();
    CHECK(range90.second == doctest::Approx(10.0)); // Проекция точки на плоскость Y даёт смещение только по радиусу
}

TEST_CASE("VerticalRenderer renders project point marker") {
    Project project;
    WellEntry entry;
    entry.id = "well-3";
    entry.visible = false; // траектория не нужна на рендере

    WellResult result;
    result.well = "Test well 3";
    entry.color = Color::red();

    ProcessedPoint p1;
    p1.depth = Meters{0.0};
    p1.x = Meters{0.0};
    p1.y = Meters{0.0};
    p1.tvd = Meters{0.0};

    ProcessedPoint p2;
    p2.depth = Meters{100.0};
    p2.x = Meters{0.0};
    p2.y = Meters{0.0};
    p2.tvd = Meters{100.0};

    result.points = {p1, p2};
    result.actual_shift = p2.calculatedShift();
    result.actual_direction_angle = Degrees{0.0};

    ProjectPoint pp;
    pp.name = "P1";
    pp.shift = Meters{50.0};
    pp.azimuth_geographic = Degrees{0.0};
    pp.radius = Meters{5.0};

    ProjectPointFactual fact;
    fact.x = Meters{50.0};
    fact.y = Meters{0.0};
    fact.tvd = Meters{100.0};
    pp.factual = fact;

    result.project_points.push_back(pp);
    entry.result = result;
    project.wells.push_back(entry);

    VerticalRenderer renderer;
    VerticalRenderSettings settings;
    settings.auto_azimuth = false;
    settings.projection_azimuth = Degrees{0.0};
    settings.show_grid = false;
    settings.show_depth_labels = false;
    settings.show_well_labels = false;
    settings.show_sea_level = false;
    settings.show_header = false;
    settings.offset_y = -50.0f; // поднимем картинку, чтобы точка была в кадре
    renderer.setSettings(settings);
    renderer.updateFromProject(project);

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 200);
    auto cr = cairo_create(surface);
    renderer.render(cr, 200, 200);
    cairo_surface_flush(surface);

    unsigned char* data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);

    int red_pixels = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            unsigned char* px = data + y * stride + x * 4; // BGRA
            unsigned char b = px[0];
            unsigned char g = px[1];
            unsigned char r = px[2];
            if (r > 200 && r > g + 50 && r > b + 50) {
                red_pixels++;
            }
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    CHECK(red_pixels > 0);
}

TEST_CASE("VerticalRenderer snapshot is stable without labels") {
    Project project;
    WellEntry entry;
    entry.id = "well-4";
    entry.visible = true;
    entry.color = Color::blue();

    WellResult result;
    result.well = "Well 4";

    ProcessedPoint p1;
    p1.depth = Meters{0.0};
    p1.x = Meters{0.0};
    p1.y = Meters{0.0};
    p1.tvd = Meters{0.0};

    ProcessedPoint p2;
    p2.depth = Meters{100.0};
    p2.x = Meters{0.0};
    p2.y = Meters{0.0};
    p2.tvd = Meters{100.0};

    result.points = {p1, p2};
    result.actual_shift = p2.calculatedShift();
    result.actual_direction_angle = Degrees{0.0};

    ProjectPoint pp;
    pp.name = "P2";
    pp.shift = Meters{40.0};
    pp.azimuth_geographic = Degrees{0.0};
    pp.radius = Meters{5.0};

    ProjectPointFactual fact;
    fact.x = Meters{40.0};
    fact.y = Meters{0.0};
    fact.tvd = Meters{80.0};
    pp.factual = fact;

    result.project_points.push_back(pp);
    entry.result = result;
    project.wells.push_back(entry);

    VerticalRenderer renderer;
    VerticalRenderSettings settings;
    settings.auto_azimuth = false;
    settings.projection_azimuth = Degrees{0.0};
    settings.show_grid = false;
    settings.show_depth_labels = false;
    settings.show_well_labels = false;
    settings.show_sea_level = false;
    settings.show_header = false;
    settings.show_project_point_labels = false;
    renderer.setSettings(settings);
    renderer.updateFromProject(project);

    auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 160, 160);
    auto cr = cairo_create(surface);
    renderer.render(cr, 160, 160);
    cairo_surface_flush(surface);

    unsigned char* data = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    int height = cairo_image_surface_get_height(surface);

    std::uint64_t hash = 0;
    for (int y = 0; y < height; ++y) {
        unsigned char* row = data + y * stride;
        for (int x = 0; x < 160 * 4; ++x) {
            hash = (hash * 131) + row[x];
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    std::uint64_t expected_hash = 9483709595325699971ULL;
    INFO("hash=" << hash);
    CHECK(hash == expected_hash);
}

#endif // INCLINE3D_HAS_GUI

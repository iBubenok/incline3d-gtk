#include <doctest/doctest.h>
#include "io/project_io.hpp"
#include "model/project.hpp"
#include "model/processed_point.hpp"
#include <filesystem>

using namespace incline::model;
using namespace incline::io;

TEST_CASE("Project save/load preserves wells and project points") {
    Project project;
    project.name = "Demo";

    WellEntry entry;
    entry.id = "well-1";
    entry.visible = true;
    entry.is_base = true;
    entry.color = Color::fromHex("#123456");

    IntervalData data;
    data.well = "W1";
    MeasurementPoint mp;
    mp.depth = Meters{0.0};
    mp.inclination = Degrees{0.0};
    mp.magnetic_azimuth = Degrees{0.0};
    data.measurements.push_back(mp);
    entry.source_data = data;

    ProjectPoint pp;
    pp.name = "PP-1";
    pp.azimuth_geographic = Degrees{30.0};
    pp.shift = Meters{120.0};
    pp.depth = Meters{250.0};
    pp.radius = Meters{25.0};
    ProjectPointFactual factual;
    factual.inclination = Degrees{5.0};
    factual.magnetic_azimuth = Degrees{28.0};
    factual.true_azimuth = Degrees{32.0};
    factual.shift = Meters{121.0};
    factual.elongation = Meters{15.0};
    factual.x = Meters{10.0};
    factual.y = Meters{20.0};
    factual.deviation = Meters{4.0};
    factual.deviation_direction = Degrees{210.0};
    factual.tvd = Meters{240.0};
    factual.intensity_10m = 0.5;
    factual.intensity_L = 0.7;
    pp.factual = factual;
    entry.project_points.push_back(pp);

    WellResult result;
    result.well = data.well;
    ProcessedPoint processed;
    processed.depth = Meters{250.0};
    processed.inclination = Degrees{5.0};
    processed.magnetic_azimuth = Degrees{28.0};
    processed.true_azimuth = Degrees{32.0};
    processed.computed_azimuth = Degrees{32.0};
    processed.x = Meters{10.0};
    processed.y = Meters{20.0};
    processed.tvd = Meters{240.0};
    processed.absg = Meters{60.0};
    processed.shift = Meters{22.0};
    processed.direction_angle = Degrees{63.0};
    processed.elongation = Meters{10.0};
    processed.intensity_10m = 0.5;
    processed.intensity_L = 0.7;
    processed.error_x = Meters{0.1};
    processed.error_y = Meters{0.1};
    processed.error_absg = Meters{0.1};
    processed.error_intensity = 0.05;
    result.points.push_back(processed);
    result.project_points = entry.project_points;
    entry.result = result;

    project.wells.push_back(entry);

    auto path = std::filesystem::temp_directory_path() / "incline3d_project_io_test.inclproj";
    saveProject(project, path);
    auto loaded = loadProject(path);

    REQUIRE(loaded.wells.size() == 1);
    const auto& loaded_well = loaded.wells.front();
    CHECK(loaded_well.id == entry.id);
    CHECK(loaded_well.project_points.size() == 1);
    const auto& loaded_pp = loaded_well.project_points.front();
    CHECK(loaded_pp.name == pp.name);
    CHECK(loaded_pp.radius.value == doctest::Approx(pp.radius.value));
    CHECK(loaded_pp.azimuth_geographic.has_value());
    CHECK(loaded_pp.depth.has_value());

    REQUIRE(loaded_well.result.has_value());
    CHECK(loaded_well.result->project_points.size() == 1);
    CHECK(loaded_well.result->project_points.front().name == pp.name);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

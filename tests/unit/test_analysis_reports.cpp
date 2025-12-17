/**
 * @file test_analysis_reports.cpp
 * @brief Проверка расчёта анализов и экспорта отчётов
 */

#include <doctest/doctest.h>
#include "core/analysis.hpp"
#include "io/analysis_report_writer.hpp"
#include <filesystem>
#include <fstream>

namespace {

using namespace incline::model;
using incline::core::AnalysesReportData;

WellResult makeVerticalWell(double x_offset, std::string name) {
    WellResult well;
    well.well = std::move(name);

    ProcessedPoint p1;
    p1.depth = Meters{0.0};
    p1.x = Meters{x_offset};
    p1.y = Meters{0.0};
    p1.tvd = Meters{0.0};

    ProcessedPoint p2;
    p2.depth = Meters{100.0};
    p2.x = Meters{x_offset};
    p2.y = Meters{0.0};
    p2.tvd = Meters{100.0};

    well.points.push_back(p1);
    well.points.push_back(p2);
    return well;
}

} // namespace

TEST_CASE("analyses report computes proximity between simple wells") {
    auto base = makeVerticalWell(0.0, "BASE");
    auto target = makeVerticalWell(50.0, "TARGET");

    auto report = incline::core::buildAnalysesReport(base, target, Meters{50.0});
    CHECK(report.valid);
    CHECK(report.proximity.isValid());
    CHECK(report.proximity.min_distance.value == doctest::Approx(50.0));
    CHECK(report.profile.size() >= 2);
}

TEST_CASE("analysis report writer creates markdown and csv") {
    auto base = makeVerticalWell(0.0, "BASE");
    auto target = makeVerticalWell(20.0, "TARGET");
    auto report = incline::core::buildAnalysesReport(base, target, Meters{50.0});

    namespace fs = std::filesystem;
    auto out_dir = fs::temp_directory_path() / "incline3d_analysis_report";
    std::error_code ec;
    fs::remove_all(out_dir, ec);

    auto result = incline::io::writeAnalysisReport(report, out_dir);
    CHECK(fs::exists(result.markdown_path));
    CHECK(fs::exists(result.csv_path));

    std::ifstream csv(result.csv_path);
    std::string header;
    std::getline(csv, header);
    CHECK(header.find("TVD") != std::string::npos);
}

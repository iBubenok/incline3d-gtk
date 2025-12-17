/**
 * @file test_diagnostics_core.cpp
 * @brief Проверки core-диагностики и записи отчётов
 */

#include <doctest/doctest.h>
#include "core/diagnostics.hpp"
#include "io/diagnostics_writer.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

TEST_CASE("core diagnostics produce report files") {
    namespace fs = std::filesystem;
    auto out_dir = fs::temp_directory_path() / "incline3d_diag_core";
    std::error_code ec;
    fs::remove_all(out_dir, ec);

    incline::core::DiagnosticsOptions options;
    options.artifacts_dir = out_dir;
    options.request_render_selftest = false;
    options.gui_available = false;

    auto report = incline::core::buildDiagnosticsReport(options, std::nullopt);
    auto summary = report.summarize();

    CHECK(summary.fail == 0);
    CHECK(report.meta.app_version.size() > 0);

    auto write_result = incline::io::writeDiagnosticsReports(report, out_dir);
    CHECK(fs::exists(write_result.json_path));
    CHECK(fs::exists(write_result.markdown_path));

    std::ifstream ifs(write_result.json_path);
    nlohmann::json j;
    ifs >> j;
    CHECK(j["schema_version"] == report.meta.schema_version);
    CHECK(j["summary"]["status"] == "OK");
}

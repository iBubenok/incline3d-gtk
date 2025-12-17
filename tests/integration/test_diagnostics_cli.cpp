/**
 * @file test_diagnostics_cli.cpp
 * @brief Интеграционный тест расширенной диагностики
 */

#include <doctest/doctest.h>
#include "app/diagnostics_runner.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#if defined(INCLINE3D_HAS_GUI)

TEST_CASE("diagnostics command produces reports and images") {
    namespace fs = std::filesystem;
    auto out_dir = fs::temp_directory_path() / "incline3d_diag_cli";
    std::error_code ec;
    fs::remove_all(out_dir, ec);

    auto res = incline::app::runDiagnosticsCommand(out_dir, true);
    CHECK(res.exit_code == 0);

    auto json_path = out_dir / "report.json";
    auto md_path = out_dir / "report.md";
    CHECK(fs::exists(json_path));
    CHECK(fs::exists(md_path));

    for (const auto& name : {"plan.png", "vertical.png", "axonometry.png"}) {
        auto path = out_dir / "images" / name;
        INFO("Файл: " << path.string());
        CHECK(fs::exists(path));
        CHECK(fs::file_size(path) > 0);
    }

    std::ifstream ifs(json_path);
    nlohmann::json j;
    ifs >> j;
    CHECK(j["summary"]["status"] == "OK");
    CHECK(j["checks"].size() >= 3);
}

#else

TEST_CASE("diagnostics CLI skipped without GUI") {
    MESSAGE("GUI отключён, интеграционная диагностика пропущена");
}

#endif

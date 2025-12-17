/**
 * @file test_render_selftest.cpp
 * @brief Smoke-тест самопроверки рендеринга
 */

#include <doctest/doctest.h>
#include "app/render_selftest.hpp"
#include <filesystem>
#include <fstream>

#if defined(INCLINE3D_HAS_GUI)

TEST_CASE("render selftest produces images") {
    namespace fs = std::filesystem;
    auto out_dir = fs::temp_directory_path() / "incline3d_selftest";
    if (fs::exists(out_dir)) {
        std::error_code ec;
        fs::remove_all(out_dir, ec);
    }

    int code = incline::app::runRenderSelfTest(out_dir);
    REQUIRE(code == 0);

    for (const auto& name : {"plan.png", "vertical.png", "axonometry.png"}) {
        auto path = out_dir / name;
        INFO("Файл: " << path.string());
        REQUIRE(fs::exists(path));
        REQUIRE(fs::file_size(path) > 0);
    }
}

#else

TEST_CASE("render selftest skipped without GUI") {
    MESSAGE("GUI отключён, самопроверка рендеринга пропущена");
}

#endif

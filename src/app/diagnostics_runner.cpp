/**
 * @file diagnostics_runner.cpp
 * @brief Запуск расширенной диагностики/selftest
 */

#include "diagnostics_runner.hpp"
#include "render_selftest.hpp"
#include "core/diagnostics.hpp"
#include "io/diagnostics_writer.hpp"
#include <iostream>
#include <optional>

namespace incline::app {
namespace {

using namespace incline::model;

DiagnosticCheck buildRenderCheck(const RenderSelfTestResult& render_result, const std::filesystem::path& images_dir) {
    DiagnosticCheck check;
    check.id = "render_selftest";
    check.title = "Визуальный selftest (рендер изображений)";

    if (render_result.success) {
        check.status = DiagnosticStatus::Ok;
        check.details = "Изображения сформированы";
        for (const auto& path : render_result.images) {
            check.artifacts.push_back({
                path.filename().string(),
                std::filesystem::path("images") / path.filename()
            });
        }
    } else {
        check.status = DiagnosticStatus::Fail;
        check.details = render_result.error_message.empty()
            ? "Не удалось выполнить рендеринг"
            : render_result.error_message;
    }

    // Убедиться, что относительные пути указывают на images/
    if (check.status == DiagnosticStatus::Ok && check.artifacts.empty()) {
        for (const auto& name : {"plan.png", "vertical.png", "axonometry.png"}) {
            check.artifacts.push_back({name, std::filesystem::path("images") / name});
        }
    }

    (void)images_dir;
    return check;
}

} // namespace

DiagnosticsCommandResult runDiagnosticsCommand(
    const std::filesystem::path& output_dir,
    bool request_images
) {
    DiagnosticsCommandResult result;
    result.output_dir = output_dir;
    std::filesystem::create_directories(output_dir);

    std::optional<DiagnosticCheck> render_check;

#if defined(INCLINE3D_HAS_GUI)
    bool can_render = request_images;
    if (can_render) {
        auto images_dir = output_dir / "images";
        auto render_result = performRenderSelfTest(images_dir);
        render_check = buildRenderCheck(render_result, images_dir);
    } else {
        DiagnosticCheck skipped;
        skipped.id = "render_selftest";
        skipped.title = "Визуальный selftest (рендер изображений)";
        skipped.status = DiagnosticStatus::Skipped;
        skipped.details = "Рендер отключён флагом --no-images";
        render_check = skipped;
    }
    bool gui_available = true;
#else
    bool gui_available = false;
#endif

    core::DiagnosticsOptions options;
    options.artifacts_dir = output_dir;
    options.request_render_selftest = true;
    options.gui_available = gui_available;

    auto report = core::buildDiagnosticsReport(options, render_check);
    auto summary = report.summarize();
    result.summary = summary;
    result.report = report;

    io::writeDiagnosticsReports(report, output_dir);

    result.exit_code = (summary.status == DiagnosticStatus::Fail) ? 1 : 0;
    return result;
}

} // namespace incline::app

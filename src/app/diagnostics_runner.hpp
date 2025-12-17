/**
 * @file diagnostics_runner.hpp
 * @brief Запуск расширенной диагностики/selftest из CLI/UI
 */

#pragma once

#include "model/diagnostics.hpp"
#include <filesystem>

namespace incline::app {

struct DiagnosticsCommandResult {
    int exit_code = 1;
    std::filesystem::path output_dir;
    incline::model::DiagnosticsSummary summary;
    incline::model::DiagnosticsReport report;
};

/**
 * @brief Выполнить диагностику и сохранить отчёты в каталог.
 * @param output_dir Каталог артефактов (report.md/json, images/)
 * @param request_images Нужно ли пытаться строить изображения selftest
 */
DiagnosticsCommandResult runDiagnosticsCommand(
    const std::filesystem::path& output_dir,
    bool request_images
);

} // namespace incline::app

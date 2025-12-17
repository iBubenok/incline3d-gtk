/**
 * @file diagnostics_writer.hpp
 * @brief Запись диагностических отчётов в Markdown и JSON
 */

#pragma once

#include "model/diagnostics.hpp"
#include <filesystem>

namespace incline::io {

struct DiagnosticsWriteResult {
    std::filesystem::path json_path;
    std::filesystem::path markdown_path;
};

/**
 * @brief Записать диагностический отчёт в указанный каталог.
 */
DiagnosticsWriteResult writeDiagnosticsReports(
    const incline::model::DiagnosticsReport& report,
    const std::filesystem::path& output_dir
);

} // namespace incline::io

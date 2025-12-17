/**
 * @file analysis_report_writer.hpp
 * @brief Экспорт отчёта анализов (proximity/offset)
 */

#pragma once

#include "core/analysis.hpp"
#include <filesystem>

namespace incline::io {

struct AnalysisExportResult {
    std::filesystem::path markdown_path;
    std::filesystem::path csv_path;
};

/**
 * @brief Записать отчёт анализов в Markdown и CSV.
 */
AnalysisExportResult writeAnalysisReport(
    const incline::core::AnalysesReportData& report,
    const std::filesystem::path& output_dir
);

} // namespace incline::io

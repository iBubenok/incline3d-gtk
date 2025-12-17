/**
 * @file analysis_report_writer.cpp
 * @brief Экспорт отчётов анализов
 */

#include "analysis_report_writer.hpp"
#include "file_utils.hpp"
#include <iomanip>
#include <sstream>

namespace incline::io {
namespace {

using namespace incline::core;

std::string formatMeters(Meters m) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << m.value;
    return oss.str();
}

std::string buildMarkdown(const AnalysesReportData& report) {
    std::ostringstream out;
    out << "# Отчёт по анализам (сближение/отход)\n\n";
    out << "- Базовая скважина: " << report.base_name << "\n";
    out << "- Целевая скважина: " << report.target_name << "\n\n";

    if (!report.valid) {
        out << "_Недостаточно обработанных данных для расчёта._\n";
        return out.str();
    }

    out << "## Сближение (Proximity)\n";
    out << "- Минимальное расстояние: " << formatMeters(report.proximity.min_distance) << " м\n";
    out << "- Глубина базовой: " << formatMeters(report.proximity.depth1) << " м\n";
    out << "- Глубина целевой: " << formatMeters(report.proximity.depth2) << " м\n";
    out << "- TVD сближения: " << formatMeters(report.proximity.tvd) << " м\n\n";

    out << "## Отход (Offset)\n";
    if (report.has_deviation) {
        out << "- Точек с фактом: " << report.deviation_stats.total_project_points << "\n";
        out << "- В допуске: " << report.deviation_stats.points_within_tolerance << "\n";
        out << "- Средний отход: " << formatMeters(report.deviation_stats.avg_deviation) << " м\n";
        out << "- Максимальный отход: " << formatMeters(report.deviation_stats.max_deviation)
            << " м на глубине " << formatMeters(report.deviation_stats.max_deviation_depth) << " м\n\n";
    } else {
        out << "_Фактические параметры проектных точек отсутствуют._\n\n";
    }

    if (!report.profile.empty()) {
        out << "## Профиль сближения по TVD\n";
        out << "| TVD (м) | Расстояние 3D (м) | Горизонтальное (м) | MD базовая (м) | MD целевая (м) |\n";
        out << "|---------|-------------------|---------------------|----------------|----------------|\n";
        for (const auto& p : report.profile) {
            out << "| " << formatMeters(p.tvd)
                << " | " << formatMeters(p.distance_3d)
                << " | " << formatMeters(p.distance_horizontal)
                << " | " << formatMeters(p.depth1)
                << " | " << formatMeters(p.depth2)
                << " |\n";
        }
        out << "\n";
    }

    return out.str();
}

std::string buildCsv(const AnalysesReportData& report) {
    std::ostringstream out;
    out << "TVD;Distance3D;DistanceHorizontal;MD_Base;MD_Target\n";
    for (const auto& p : report.profile) {
        out << formatMeters(p.tvd) << ";"
            << formatMeters(p.distance_3d) << ";"
            << formatMeters(p.distance_horizontal) << ";"
            << formatMeters(p.depth1) << ";"
            << formatMeters(p.depth2) << "\n";
    }
    return out.str();
}

} // namespace

AnalysisExportResult writeAnalysisReport(
    const AnalysesReportData& report,
    const std::filesystem::path& output_dir
) {
    std::filesystem::create_directories(output_dir);
    AnalysisExportResult result;

    auto md_path = output_dir / "analysis_report.md";
    auto csv_path = output_dir / "analysis_profile.csv";

    atomicWrite(md_path, buildMarkdown(report));
    atomicWrite(csv_path, buildCsv(report));

    result.markdown_path = md_path;
    result.csv_path = csv_path;
    return result;
}

} // namespace incline::io

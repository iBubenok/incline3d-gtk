/**
 * @file diagnostics_writer.cpp
 * @brief Запись диагностических отчётов
 */

#include "diagnostics_writer.hpp"
#include "file_utils.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

namespace incline::io {
namespace {

using namespace incline::model;

std::string statusToMarkdown(DiagnosticStatus status) {
    if (status == DiagnosticStatus::Ok) return "OK";
    if (status == DiagnosticStatus::Warning) return "WARN";
    if (status == DiagnosticStatus::Fail) return "FAIL";
    return "SKIPPED";
}

std::string buildMarkdown(const DiagnosticsReport& report) {
    std::ostringstream out;
    auto summary = report.summarize();

    out << "# Диагностический отчёт Incline3D\n\n";
    out << "- Версия приложения: " << report.meta.app_version << "\n";
    out << "- Тип сборки: " << report.meta.build_type << "\n";
    out << "- Платформа: " << report.meta.platform << "\n";
    out << "- GUI: " << (report.meta.gui_enabled ? "да" : "нет") << "\n";
    out << "- Схема отчёта: " << report.meta.schema_version << "\n";
    out << "- Каталог артефактов: " << report.meta.artifacts_root.string() << "\n";
    out << "- Время: " << report.meta.timestamp << "\n\n";

    out << "## Сводка\n";
    out << "- Статус: " << statusToMarkdown(summary.status) << "\n";
    out << "- OK: " << summary.ok << ", WARN: " << summary.warning
        << ", FAIL: " << summary.fail << ", SKIPPED: " << summary.skipped << "\n\n";

    out << "## Проверки\n";
    out << "| Проверка | Статус | Детали |\n";
    out << "|----------|--------|--------|\n";
    for (const auto& check : report.checks) {
        out << "| " << check.title << " | " << statusToMarkdown(check.status)
            << " | " << check.details << " |\n";
    }
    out << "\n";

    out << "## Артефакты\n";
    for (const auto& check : report.checks) {
        if (check.artifacts.empty()) continue;
        out << "- " << check.title << ":\n";
        for (const auto& art : check.artifacts) {
            out << "  - " << art.name << ": " << art.relative_path.string() << "\n";
        }
    }

    return out.str();
}

nlohmann::json buildJson(const DiagnosticsReport& report) {
    nlohmann::json j;
    auto summary = report.summarize();

    j["schema_version"] = report.meta.schema_version;
    j["meta"] = {
        {"app_version", report.meta.app_version},
        {"build_type", report.meta.build_type},
        {"platform", report.meta.platform},
        {"gui_enabled", report.meta.gui_enabled},
        {"timestamp", report.meta.timestamp},
        {"artifacts_root", report.meta.artifacts_root.string()}
    };

    j["checks"] = nlohmann::json::array();
    for (const auto& check : report.checks) {
        nlohmann::json c;
        c["id"] = check.id;
        c["title"] = check.title;
        c["status"] = diagnosticStatusToString(check.status);
        c["details"] = check.details;

        c["artifacts"] = nlohmann::json::array();
        for (const auto& art : check.artifacts) {
            c["artifacts"].push_back({
                {"name", art.name},
                {"path", art.relative_path.string()}
            });
        }
        j["checks"].push_back(c);
    }

    j["summary"] = {
        {"status", diagnosticStatusToString(summary.status)},
        {"ok", summary.ok},
        {"warning", summary.warning},
        {"fail", summary.fail},
        {"skipped", summary.skipped}
    };

    return j;
}

} // namespace

DiagnosticsWriteResult writeDiagnosticsReports(
    const DiagnosticsReport& report,
    const std::filesystem::path& output_dir
) {
    std::filesystem::create_directories(output_dir);
    DiagnosticsWriteResult result;

    auto json_path = output_dir / "report.json";
    auto md_path = output_dir / "report.md";

    auto json_text = buildJson(report).dump(2);
    auto md_text = buildMarkdown(report);

    atomicWrite(json_path, json_text);
    atomicWrite(md_path, md_text);

    result.json_path = json_path;
    result.markdown_path = md_path;
    return result;
}

} // namespace incline::io

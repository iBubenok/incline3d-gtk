/**
 * @file diagnostics.hpp
 * @brief Структуры данных для диагностического отчёта
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace incline::model {

enum class DiagnosticStatus {
    Ok,
    Warning,
    Fail,
    Skipped
};

struct DiagnosticArtifact {
    std::string name;
    std::filesystem::path relative_path;
};

struct DiagnosticCheck {
    std::string id;
    std::string title;
    DiagnosticStatus status = DiagnosticStatus::Skipped;
    std::string details;
    std::vector<DiagnosticArtifact> artifacts;
};

struct DiagnosticsMeta {
    std::string schema_version = "1.0.0";
    std::string app_version;
    std::string build_type;
    std::string platform;
    bool gui_enabled = false;
    std::string timestamp;
    std::filesystem::path artifacts_root;
};

struct DiagnosticsSummary {
    DiagnosticStatus status = DiagnosticStatus::Ok;
    size_t ok = 0;
    size_t warning = 0;
    size_t fail = 0;
    size_t skipped = 0;
};

struct DiagnosticsReport {
    DiagnosticsMeta meta;
    std::vector<DiagnosticCheck> checks;

    [[nodiscard]] DiagnosticsSummary summarize() const noexcept {
        DiagnosticsSummary summary;
        for (const auto& check : checks) {
            switch (check.status) {
            case DiagnosticStatus::Ok: summary.ok++; break;
            case DiagnosticStatus::Warning: summary.warning++; break;
            case DiagnosticStatus::Fail: summary.fail++; break;
            case DiagnosticStatus::Skipped: summary.skipped++; break;
            }
        }
        if (summary.fail > 0) {
            summary.status = DiagnosticStatus::Fail;
        } else if (summary.warning > 0) {
            summary.status = DiagnosticStatus::Warning;
        } else if (summary.ok > 0 && summary.warning == 0 && summary.fail == 0) {
            summary.status = DiagnosticStatus::Ok;
        } else {
            summary.status = DiagnosticStatus::Skipped;
        }
        return summary;
    }
};

[[nodiscard]] inline std::string_view diagnosticStatusToString(DiagnosticStatus status) noexcept {
    switch (status) {
    case DiagnosticStatus::Ok: return "OK";
    case DiagnosticStatus::Warning: return "WARN";
    case DiagnosticStatus::Fail: return "FAIL";
    case DiagnosticStatus::Skipped: return "SKIPPED";
    }
    return "UNKNOWN";
}

} // namespace incline::model

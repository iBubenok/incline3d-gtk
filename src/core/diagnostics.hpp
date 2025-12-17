/**
 * @file diagnostics.hpp
 * @brief Запуск диагностических проверок (core, без GTK)
 */

#pragma once

#include "model/diagnostics.hpp"
#include <filesystem>
#include <optional>

namespace incline::core {

struct DiagnosticsOptions {
    std::filesystem::path artifacts_dir;       ///< Корень каталога артефактов
    bool request_render_selftest = true;       ///< Нужно ли добавлять пункт про визуальный selftest
    bool gui_available = false;                ///< Доступен ли GUI/рендер
};

/**
 * @brief Построить диагностический отчёт по core-проверкам.
 *
 * @param options Настройки выполнения
 * @param render_check Результат визуальной проверки (если GUI доступен). Если не задан, будет добавлен SKIPPED.
 */
[[nodiscard]] model::DiagnosticsReport buildDiagnosticsReport(
    const DiagnosticsOptions& options,
    std::optional<model::DiagnosticCheck> render_check = std::nullopt
);

} // namespace incline::core

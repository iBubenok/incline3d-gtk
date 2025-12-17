/**
 * @file diagnostics.cpp
 * @brief Реализация диагностических проверок (core)
 */

#include "diagnostics.hpp"
#include "processing.hpp"
#include "model/project.hpp"
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>

namespace incline::core {
namespace {

using namespace incline::model;

std::string isoTimestampNow() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time_t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
}

std::string detectPlatform() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

DiagnosticCheck makeBuildInfoCheck(const DiagnosticsMeta& meta) {
    DiagnosticCheck check;
    check.id = "build_info";
    check.title = "Сборка и версия";
    check.status = DiagnosticStatus::Ok;

    std::ostringstream oss;
    oss << "Версия: " << meta.app_version
        << ", сборка: " << meta.build_type
        << ", платформа: " << meta.platform
        << ", GUI: " << (meta.gui_enabled ? "да" : "нет");
    check.details = oss.str();
    return check;
}

DiagnosticCheck makeFilesystemCheck(const std::filesystem::path& artifacts_dir) {
    DiagnosticCheck check;
    check.id = "filesystem";
    check.title = "Запись и чтение на диске";
    check.status = DiagnosticStatus::Fail;

    try {
        auto logs_dir = artifacts_dir / "logs";
        std::filesystem::create_directories(logs_dir);
        auto probe_path = logs_dir / "fs_probe.txt";

        {
            std::ofstream ofs(probe_path, std::ios::binary);
            ofs << "incline3d diagnostics probe";
        }

        if (std::filesystem::exists(probe_path) && std::filesystem::file_size(probe_path) > 0) {
            check.status = DiagnosticStatus::Ok;
            check.details = "Запись/чтение в каталоге артефактов работает";
            check.artifacts.push_back({"probe", std::filesystem::path("logs") / "fs_probe.txt"});
        } else {
            check.details = "Файл проверки не создан или пуст";
        }
    } catch (const std::exception& ex) {
        check.details = std::string("Ошибка файловой системы: ") + ex.what();
    }

    return check;
}

IntervalData makeSampleData() {
    IntervalData data;
    data.well = "DIAG-SAMPLE";
    data.cluster = "Demo";
    data.rotor_table_altitude = Meters{200.0};
    data.magnetic_declination = Degrees{7.5};

    auto addPoint = [&data](double depth, double inc, double az) {
        MeasurementPoint m;
        m.depth = Meters{depth};
        m.inclination = Degrees{inc};
        m.magnetic_azimuth = Degrees{az};
        data.measurements.push_back(m);
    };

    addPoint(0.0, 0.0, 5.0);
    addPoint(100.0, 8.0, 20.0);
    addPoint(200.0, 25.0, 60.0);
    addPoint(300.0, 60.0, 95.0);
    addPoint(400.0, 90.0, 120.0);
    addPoint(500.0, 90.0, 130.0);
    return data;
}

DiagnosticCheck makeProcessingCheck() {
    DiagnosticCheck check;
    check.id = "processing";
    check.title = "Расчёт траектории на эталонных данных";
    check.status = DiagnosticStatus::Fail;

    try {
        ProcessingOptions options;
        options.method = TrajectoryMethod::MinimumCurvatureIntegral;
        options.azimuth_mode = AzimuthMode::Magnetic;
        options.dogleg_method = DoglegMethod::Sine;
        options.interpolate_missing_azimuths = true;
        options.extend_last_azimuth = true;
        options.blank_vertical_azimuth = true;
        options.vertical_if_no_azimuth = true;

        auto result = processWell(makeSampleData(), options);
        if (result.points.empty()) {
            check.details = "Результат пустой";
            return check;
        }

        bool finite = true;
        for (const auto& p : result.points) {
            finite = finite && std::isfinite(p.x.value) && std::isfinite(p.y.value) && std::isfinite(p.tvd.value);
            if (!finite) break;
        }

        if (finite) {
            check.status = DiagnosticStatus::Ok;
            check.details = "Все координаты расчёта конечные, без NaN/inf";
        } else {
            check.details = "Обнаружены некорректные координаты";
        }
    } catch (const std::exception& ex) {
        check.details = std::string("Ошибка расчёта: ") + ex.what();
    }

    return check;
}

DiagnosticCheck makeInvalidInputCheck() {
    DiagnosticCheck check;
    check.id = "invalid_input";
    check.title = "Обработка пустых/неполных данных";
    check.status = DiagnosticStatus::Fail;

    try {
        IntervalData empty;
        ProcessingOptions options;
        auto result = processWell(empty, options);

        if (result.points.empty()) {
            check.status = DiagnosticStatus::Ok;
            check.details = "Пустой вход обрабатывается без сбоев";
        } else {
            check.details = "Ожидался пустой результат на пустых данных";
        }
    } catch (const std::exception& ex) {
        check.details = std::string("Исключение на пустых данных: ") + ex.what();
    }

    return check;
}

DiagnosticCheck makeRenderPlaceholderCheck(bool gui_available) {
    DiagnosticCheck check;
    check.id = "render_selftest";
    check.title = "Визуальный selftest (рендер изображений)";
    check.status = DiagnosticStatus::Skipped;
    if (gui_available) {
        check.details = "Результат рендера не передан";
    } else {
        check.details = "GUI отключён (BUILD_GUI=OFF)";
    }
    return check;
}

} // namespace

model::DiagnosticsReport buildDiagnosticsReport(
    const DiagnosticsOptions& options,
    std::optional<model::DiagnosticCheck> render_check
) {
    DiagnosticsReport report;
    report.meta.app_version = INCLINE3D_VERSION;
    report.meta.build_type = INCLINE3D_BUILD_TYPE;
    report.meta.platform = detectPlatform();
    report.meta.gui_enabled = options.gui_available;
    report.meta.timestamp = isoTimestampNow();
    report.meta.artifacts_root = options.artifacts_dir;

    report.checks.push_back(makeBuildInfoCheck(report.meta));
    report.checks.push_back(makeFilesystemCheck(options.artifacts_dir));
    report.checks.push_back(makeProcessingCheck());
    report.checks.push_back(makeInvalidInputCheck());

    if (options.request_render_selftest) {
        if (render_check.has_value()) {
            report.checks.push_back(*render_check);
        } else {
            report.checks.push_back(makeRenderPlaceholderCheck(options.gui_available));
        }
    }

    return report;
}

} // namespace incline::core

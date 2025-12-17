/**
 * @file main.cpp
 * @brief Точка входа приложения Incline3D
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "ui/application.hpp"
#include "render_selftest.hpp"
#include "diagnostics_runner.hpp"
#include "core/analysis.hpp"
#include "core/processing.hpp"
#include "io/analysis_report_writer.hpp"
#include "model/interval_data.hpp"
#include <iostream>
#include <filesystem>
#include <string_view>
#include <vector>

namespace {

using namespace incline::model;
using namespace incline::core;

WellResult makeSampleWell(double azimuth_shift, const std::string& name) {
    IntervalData data;
    data.well = name;
    data.cluster = "Анализ";
    data.rotor_table_altitude = Meters{150.0};
    data.magnetic_declination = Degrees{7.0};

    auto addPoint = [&data, azimuth_shift](double depth, double inc, double az) {
        MeasurementPoint m;
        m.depth = Meters{depth};
        m.inclination = Degrees{inc};
        m.magnetic_azimuth = Degrees{az + azimuth_shift};
        data.measurements.push_back(m);
    };

    addPoint(0.0, 0.0, 0.0);
    addPoint(100.0, 5.0, 20.0);
    addPoint(200.0, 20.0, 60.0);
    addPoint(300.0, 60.0, 90.0);
    addPoint(400.0, 90.0, 120.0);

    ProcessingOptions options;
    options.method = TrajectoryMethod::MinimumCurvatureIntegral;
    options.azimuth_mode = AzimuthMode::Magnetic;
    options.dogleg_method = DoglegMethod::Sine;
    options.intensity_interval_L = Meters{25.0};
    options.interpolate_missing_azimuths = true;
    options.extend_last_azimuth = true;
    options.blank_vertical_azimuth = true;
    options.vertical_if_no_azimuth = true;

    return processWell(data, options);
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        // Режим самопроверки рендеринга: --render-selftest [путь]
        if (argc >= 2 && std::string_view(argv[1]) == "--render-selftest") {
            std::filesystem::path out_dir = (argc >= 3) ? std::filesystem::path(argv[2]) : std::filesystem::path("render-selftest");
            int code = incline::app::runRenderSelfTest(out_dir);
            if (code != 0) {
                std::cerr << "Самопроверка рендеринга завершилась с ошибкой" << std::endl;
            }
            return code;
        }

        // Расширенная диагностика: --diagnostics [--out <путь>] [--no-images]
        if (argc >= 2 && std::string_view(argv[1]) == "--diagnostics") {
            std::filesystem::path out_dir;
            bool request_images = true;

            for (int i = 2; i < argc; ++i) {
                std::string_view arg(argv[i]);
                if (arg == "--out" && i + 1 < argc) {
                    out_dir = std::filesystem::path(argv[i + 1]);
                    ++i;
                } else if (arg == "--no-images") {
                    request_images = false;
                }
            }

            if (out_dir.empty()) {
                out_dir = std::filesystem::temp_directory_path() / "incline3d_diagnostics";
            }

            auto result = incline::app::runDiagnosticsCommand(out_dir, request_images);
            if (result.exit_code == 0) {
                std::cout << "Диагностика завершена: " << out_dir << std::endl;
            } else {
                std::cerr << "Диагностика завершилась с ошибками: " << out_dir << std::endl;
            }
            return result.exit_code;
        }

        // Отчёт по анализам (proximity/offset): --report-analyses [--out <путь>]
        if (argc >= 2 && std::string_view(argv[1]) == "--report-analyses") {
            std::filesystem::path out_dir;
            for (int i = 2; i < argc; ++i) {
                std::string_view arg(argv[i]);
                if (arg == "--out" && i + 1 < argc) {
                    out_dir = std::filesystem::path(argv[i + 1]);
                    ++i;
                }
            }

            if (out_dir.empty()) {
                out_dir = std::filesystem::temp_directory_path() / "incline3d_analyses";
            }

            auto base = makeSampleWell(0.0, "BASE-SAMPLE");
            auto target = makeSampleWell(25.0, "TARGET-SAMPLE");
            auto analysis_report = incline::core::buildAnalysesReport(
                base,
                target,
                incline::model::Meters{50.0}
            );
            incline::io::writeAnalysisReport(analysis_report, out_dir);

            std::cout << "Отчёт анализов сохранён в: " << out_dir << std::endl;
            return 0;
        }

        incline::ui::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    }
}

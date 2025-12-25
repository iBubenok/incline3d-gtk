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
#include "io/format_registry.hpp"
#include "io/csv_reader.hpp"
#include "io/las_reader.hpp"
#include "model/interval_data.hpp"
#include <iostream>
#include <filesystem>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cctype>

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
        // Импорт файла без GUI: --import-measurements <файл> [опции]
        if (argc >= 3 && std::string_view(argv[1]) == "--import-measurements") {
            std::filesystem::path input_path = std::filesystem::path(argv[2]);
            std::string format_hint;

            incline::io::CsvReadOptions csv_options;
            incline::io::LasReadOptions las_options;
            bool las_manual = false;

            for (int i = 3; i < argc; ++i) {
                std::string_view arg(argv[i]);
                if (arg == "--format" && i + 1 < argc) {
                    format_hint = argv[++i];
                } else if (arg == "--depth-col" && i + 1 < argc) {
                    size_t col = std::stoul(argv[++i]);
                    if (col == 0) {
                        throw std::invalid_argument("Номер колонки глубины должен начинаться с 1");
                    }
                    csv_options.mapping.depth_column = col - 1;
                } else if (arg == "--inc-col" && i + 1 < argc) {
                    size_t col = std::stoul(argv[++i]);
                    if (col == 0) {
                        throw std::invalid_argument("Номер колонки зенитного угла должен начинаться с 1");
                    }
                    csv_options.mapping.inclination_column = col - 1;
                } else if (arg == "--delimiter" && i + 1 < argc) {
                    std::string value = argv[++i];
                    if (value == "tab") {
                        csv_options.delimiter = '\t';
                    } else if (value == "pipe") {
                        csv_options.delimiter = '|';
                    } else if (value == "comma") {
                        csv_options.delimiter = ',';
                    } else if (!value.empty()) {
                        csv_options.delimiter = value.front();
                    }
                } else if (arg == "--decimal" && i + 1 < argc) {
                    std::string value = argv[++i];
                    if (!value.empty()) {
                        csv_options.decimal_separator = value.front();
                    }
                } else if (arg == "--encoding" && i + 1 < argc) {
                    csv_options.encoding = argv[++i];
                } else if (arg == "--depth-mnemonic" && i + 1 < argc) {
                    las_options.mnemonics.depth = argv[++i];
                    las_manual = true;
                } else if (arg == "--inc-mnemonic" && i + 1 < argc) {
                    las_options.mnemonics.inclination = argv[++i];
                    las_manual = true;
                } else if (arg == "--az-mnemonic" && i + 1 < argc) {
                    las_options.mnemonics.azimuth = argv[++i];
                    las_manual = true;
                } else if (arg == "--true-az-mnemonic" && i + 1 < argc) {
                    las_options.mnemonics.true_azimuth = argv[++i];
                    las_manual = true;
                }
            }

            if (las_manual) {
                las_options.auto_detect_curves = false;
            }

            incline::io::FileFormat target_format = incline::io::FileFormat::Unknown;
            if (!format_hint.empty()) {
                std::string lowered = format_hint;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lowered == "csv") {
                    target_format = incline::io::FileFormat::CSV;
                } else if (lowered == "las") {
                    target_format = incline::io::FileFormat::LAS;
                } else if (lowered == "zak") {
                    target_format = incline::io::FileFormat::ZAK;
                }
            } else {
                auto detection = incline::io::detectFormat(input_path);
                target_format = detection.format;
                if (target_format == incline::io::FileFormat::Unknown) {
                    std::cerr << "Не удалось определить формат файла: " << detection.error_message << std::endl;
                    return 1;
                }
            }

            try {
                switch (target_format) {
                    case incline::io::FileFormat::CSV: {
                        auto data = incline::io::readCsvMeasurements(input_path, csv_options);
                        std::cout << "Импорт CSV завершён: " << data.measurements.size()
                                  << " точек. Скважина: " << data.displayName() << std::endl;
                        return 0;
                    }
                    case incline::io::FileFormat::LAS: {
                        auto data = incline::io::readLasMeasurements(input_path, las_options);
                        std::cout << "Импорт LAS завершён: " << data.measurements.size()
                                  << " точек. Скважина: " << data.displayName() << std::endl;
                        return 0;
                    }
                    default:
                        std::cerr << "Неподдерживаемый формат файла для --import-measurements" << std::endl;
                        return 1;
                }
            } catch (const std::exception& e) {
                std::cerr << "Ошибка импорта: " << e.what() << std::endl;
                return 1;
            }
        }

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

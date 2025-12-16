/**
 * @file las_writer.cpp
 * @brief Реализация экспорта данных в LAS 2.0
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "las_writer.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <ctime>

namespace incline::io {

namespace {

std::string getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream ss;
    ss << std::setfill('0')
       << std::setw(2) << tm.tm_mday << "-"
       << std::setw(2) << (tm.tm_mon + 1) << "-"
       << (1900 + tm.tm_year);
    return ss.str();
}

std::string formatLasValue(double value, int precision, double null_value) {
    if (std::isnan(value)) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(precision) << null_value;
        return ss.str();
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

std::string formatLasLine(
    std::string_view mnemonic,
    std::string_view unit,
    std::string_view value,
    std::string_view description
) {
    std::ostringstream ss;
    ss << std::left << std::setw(4) << mnemonic << "."
       << std::setw(5) << unit
       << std::setw(16) << value << ": "
       << description;
    return ss.str();
}

double getPointValue(const ProcessedPoint& pt, LasCurve curve, double rotor_altitude, double null_value) {
    switch (curve) {
        case LasCurve::Depth:
            return pt.depth.value;
        case LasCurve::Inclination:
            return pt.inclination.value;
        case LasCurve::MagneticAzimuth:
            return pt.magnetic_azimuth.has_value() ? pt.magnetic_azimuth->value : null_value;
        case LasCurve::TrueAzimuth:
            return pt.true_azimuth.has_value() ? pt.true_azimuth->value : null_value;
        case LasCurve::TVD:
            return pt.tvd.value;
        case LasCurve::TVDSS:
            return rotor_altitude - pt.tvd.value;
        case LasCurve::North:
            return pt.x.value;
        case LasCurve::East:
            return pt.y.value;
        case LasCurve::DLS:
            // DLS в °/30м = intensity_10m * 3
            return pt.intensity_10m * 3.0;
        default:
            return null_value;
    }
}

} // anonymous namespace

std::string_view getLasCurveMnemonic(LasCurve curve) noexcept {
    switch (curve) {
        case LasCurve::Depth: return "DEPT";
        case LasCurve::Inclination: return "INCL";
        case LasCurve::MagneticAzimuth: return "AZIM";
        case LasCurve::TrueAzimuth: return "AZIT";
        case LasCurve::TVD: return "TVD";
        case LasCurve::TVDSS: return "TVDSS";
        case LasCurve::North: return "NORTH";
        case LasCurve::East: return "EAST";
        case LasCurve::DLS: return "DLS";
        default: return "???";
    }
}

std::string_view getLasCurveUnit(LasCurve curve) noexcept {
    switch (curve) {
        case LasCurve::Depth:
        case LasCurve::TVD:
        case LasCurve::TVDSS:
        case LasCurve::North:
        case LasCurve::East:
            return "M";
        case LasCurve::Inclination:
        case LasCurve::MagneticAzimuth:
        case LasCurve::TrueAzimuth:
            return "DEG";
        case LasCurve::DLS:
            return "DEG/30M";
        default:
            return "";
    }
}

std::string_view getLasCurveDescription(LasCurve curve) noexcept {
    switch (curve) {
        case LasCurve::Depth: return "Measured Depth";
        case LasCurve::Inclination: return "Inclination";
        case LasCurve::MagneticAzimuth: return "Magnetic Azimuth";
        case LasCurve::TrueAzimuth: return "True Azimuth";
        case LasCurve::TVD: return "True Vertical Depth";
        case LasCurve::TVDSS: return "TVD Sub Sea";
        case LasCurve::North: return "Northing";
        case LasCurve::East: return "Easting";
        case LasCurve::DLS: return "Dogleg Severity";
        default: return "";
    }
}

std::vector<LasCurve> getDefaultLasCurves() noexcept {
    return {
        LasCurve::Depth,
        LasCurve::Inclination,
        LasCurve::MagneticAzimuth,
        LasCurve::TrueAzimuth,
        LasCurve::TVD,
        LasCurve::North,
        LasCurve::East,
        LasCurve::DLS
    };
}

void writeLas(
    const WellResult& result,
    const std::filesystem::path& path,
    const LasExportOptions& options
) {
    if (result.points.empty()) {
        throw LasWriteError("Нет данных для экспорта");
    }

    // Определяем кривые
    std::vector<LasCurve> curves = options.curves;
    if (curves.empty()) {
        curves = getDefaultLasCurves();
    }

    // Атомарная запись
    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    std::ofstream file(temp_path);
    if (!file) {
        throw LasWriteError("Не удалось создать файл: " + path.string());
    }

    // === VERSION INFORMATION ===
    file << "~VERSION INFORMATION\n";
    file << formatLasLine("VERS", "", "2.0", "CWLS LOG ASCII STANDARD - VERSION 2.0") << "\n";
    file << formatLasLine("WRAP", "", options.wrap ? "YES" : "NO", "ONE LINE PER DEPTH STEP") << "\n";
    file << "\n";

    // === WELL INFORMATION ===
    file << "~WELL INFORMATION\n";

    double start_depth = result.points.front().depth.value;
    double stop_depth = result.points.back().depth.value;

    file << formatLasLine("STRT", "M", formatLasValue(start_depth, 1, options.null_value), "START DEPTH") << "\n";
    file << formatLasLine("STOP", "M", formatLasValue(stop_depth, 1, options.null_value), "STOP DEPTH") << "\n";
    file << formatLasLine("STEP", "M", "0.0", "STEP (0=IRREGULAR)") << "\n";
    file << formatLasLine("NULL", "", formatLasValue(options.null_value, 2, options.null_value), "NULL VALUE") << "\n";

    if (!options.company.empty()) {
        file << formatLasLine("COMP", "", options.company, "COMPANY") << "\n";
    }

    file << formatLasLine("WELL", "", result.well, "WELL") << "\n";
    file << formatLasLine("FLD", "", result.field, "FIELD") << "\n";
    file << formatLasLine("LOC", "", result.cluster, "LOCATION") << "\n";

    if (!options.country.empty()) {
        file << formatLasLine("CTRY", "", options.country, "COUNTRY") << "\n";
    }

    if (!options.service_company.empty()) {
        file << formatLasLine("SRVC", "", options.service_company, "SERVICE COMPANY") << "\n";
    }

    std::string date = options.date.empty() ? getCurrentDate() : options.date;
    file << formatLasLine("DATE", "", date, "LOG DATE") << "\n";
    file << "\n";

    // === CURVE INFORMATION ===
    file << "~CURVE INFORMATION\n";
    for (const auto& curve : curves) {
        file << formatLasLine(
            getLasCurveMnemonic(curve),
            getLasCurveUnit(curve),
            "",
            getLasCurveDescription(curve)
        ) << "\n";
    }
    file << "\n";

    // === PARAMETER INFORMATION ===
    file << "~PARAMETER INFORMATION\n";
    file << formatLasLine("ALT", "M",
        formatLasValue(result.rotor_table_altitude.value, 2, options.null_value),
        "ROTARY TABLE ALTITUDE") << "\n";

    if (result.magnetic_declination.value != 0.0) {
        file << formatLasLine("DECL", "DEG",
            formatLasValue(result.magnetic_declination.value, 2, options.null_value),
            "MAGNETIC DECLINATION") << "\n";
    }

    // Метод траектории
    std::string method_name;
    switch (result.trajectory_method) {
        case TrajectoryMethod::AverageAngle: method_name = "Average Angle"; break;
        case TrajectoryMethod::BalancedTangential: method_name = "Balanced Tangential"; break;
        case TrajectoryMethod::MinimumCurvature: method_name = "Minimum Curvature"; break;
        case TrajectoryMethod::MinimumCurvatureIntegral: method_name = "Minimum Curvature (Integral)"; break;
        case TrajectoryMethod::RingArc: method_name = "Ring Arc"; break;
    }
    file << formatLasLine("METH", "", method_name, "TRAJECTORY METHOD") << "\n";
    file << "\n";

    // === ASCII LOG DATA ===
    file << "~ASCII LOG DATA\n";

    double rotor_alt = result.rotor_table_altitude.value;

    for (const auto& pt : result.points) {
        std::ostringstream line;

        for (size_t i = 0; i < curves.size(); ++i) {
            if (i > 0) line << " ";

            double value = getPointValue(pt, curves[i], rotor_alt, options.null_value);
            line << std::setw(12) << formatLasValue(value, options.decimal_places, options.null_value);
        }

        file << line.str() << "\n";
    }

    file.close();

    // Атомарное переименование
    try {
        std::filesystem::rename(temp_path, path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::filesystem::remove(temp_path);
        throw LasWriteError("Ошибка сохранения файла: " + std::string(e.what()));
    }
}

} // namespace incline::io

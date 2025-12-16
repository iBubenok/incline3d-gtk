/**
 * @file csv_writer.cpp
 * @brief Реализация экспорта данных в CSV
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "csv_writer.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace incline::io {

namespace {

std::string convertUtf8ToCp1251(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = input[i];

        if (c < 0x80) {
            result += static_cast<char>(c);
            continue;
        }

        // UTF-8 кириллица: D0 80 - D1 BF
        if (c == 0xD0 && i + 1 < input.size()) {
            unsigned char c2 = input[i + 1];
            if (c2 >= 0x90 && c2 <= 0xBF) {
                // А-Я, а-п
                result += static_cast<char>(c2 - 0x90 + 0xC0);
                ++i;
                continue;
            } else if (c2 == 0x81) {
                // Ё
                result += static_cast<char>(0xA8);
                ++i;
                continue;
            } else if (c2 >= 0x80 && c2 <= 0x8F) {
                // Специальные символы
                result += '?';
                ++i;
                continue;
            }
        } else if (c == 0xD1 && i + 1 < input.size()) {
            unsigned char c2 = input[i + 1];
            if (c2 >= 0x80 && c2 <= 0x8F) {
                // р-я
                result += static_cast<char>(c2 - 0x80 + 0xF0);
                ++i;
                continue;
            } else if (c2 == 0x91) {
                // ё
                result += static_cast<char>(0xB8);
                ++i;
                continue;
            }
        }

        // Пропускаем непонятные последовательности
        result += '?';
    }

    return result;
}

std::string formatDouble(double value, int precision, char decimal_sep) {
    if (std::isnan(value)) {
        return "";
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    std::string result = ss.str();

    if (decimal_sep != '.') {
        for (char& c : result) {
            if (c == '.') c = decimal_sep;
        }
    }

    return result;
}

std::string formatOptionalDegrees(const OptionalAngle& angle, int precision, char decimal_sep) {
    if (!angle.has_value()) {
        return "";
    }
    return formatDouble(angle->value, precision, decimal_sep);
}

} // anonymous namespace

std::string_view getFieldNameRu(ExportField field) noexcept {
    switch (field) {
        case ExportField::Depth: return "Глубина";
        case ExportField::Inclination: return "Угол";
        case ExportField::MagneticAzimuth: return "Азимут_магн";
        case ExportField::TrueAzimuth: return "Азимут_ист";
        case ExportField::X: return "X";
        case ExportField::Y: return "Y";
        case ExportField::TVD: return "TVD";
        case ExportField::ABSG: return "АБСГ";
        case ExportField::Shift: return "Смещ";
        case ExportField::DirectionAngle: return "ДирУгол";
        case ExportField::Elongation: return "Удлин";
        case ExportField::Intensity10m: return "Интенс10";
        case ExportField::IntensityL: return "ИнтенсL";
        case ExportField::Rotation: return "ВРАЩ";
        case ExportField::ROP: return "СКОР";
        case ExportField::ErrorX: return "ПогрX";
        case ExportField::ErrorY: return "ПогрY";
        case ExportField::ErrorABSG: return "ПогрАБСГ";
        case ExportField::Marker: return "Метка";
        default: return "???";
    }
}

std::string_view getFieldNameEn(ExportField field) noexcept {
    switch (field) {
        case ExportField::Depth: return "Depth";
        case ExportField::Inclination: return "Inc";
        case ExportField::MagneticAzimuth: return "Azim_Mag";
        case ExportField::TrueAzimuth: return "Azim_True";
        case ExportField::X: return "X";
        case ExportField::Y: return "Y";
        case ExportField::TVD: return "TVD";
        case ExportField::ABSG: return "ABSG";
        case ExportField::Shift: return "Shift";
        case ExportField::DirectionAngle: return "DirAngle";
        case ExportField::Elongation: return "Elong";
        case ExportField::Intensity10m: return "Int10m";
        case ExportField::IntensityL: return "IntL";
        case ExportField::Rotation: return "Rot";
        case ExportField::ROP: return "ROP";
        case ExportField::ErrorX: return "ErrX";
        case ExportField::ErrorY: return "ErrY";
        case ExportField::ErrorABSG: return "ErrABSG";
        case ExportField::Marker: return "Marker";
        default: return "???";
    }
}

std::vector<ExportField> getDefaultExportFields() noexcept {
    return {
        ExportField::Depth,
        ExportField::Inclination,
        ExportField::MagneticAzimuth,
        ExportField::TrueAzimuth,
        ExportField::X,
        ExportField::Y,
        ExportField::TVD,
        ExportField::ABSG,
        ExportField::Shift,
        ExportField::DirectionAngle,
        ExportField::Elongation,
        ExportField::Intensity10m,
        ExportField::IntensityL
    };
}

std::vector<ExportField> getMinimalExportFields() noexcept {
    return {
        ExportField::Depth,
        ExportField::Inclination,
        ExportField::MagneticAzimuth,
        ExportField::X,
        ExportField::Y,
        ExportField::TVD,
        ExportField::ABSG
    };
}

void writeCsvResults(
    const WellResult& result,
    const std::filesystem::path& path,
    const CsvExportOptions& options
) {
    // Определяем поля для экспорта
    std::vector<ExportField> fields = options.fields;
    if (fields.empty()) {
        fields = getDefaultExportFields();
    }

    // Атомарная запись: сначала во временный файл
    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    std::ofstream file(temp_path);
    if (!file) {
        throw CsvWriteError("Не удалось создать файл: " + path.string());
    }

    auto writeLine = [&](const std::string& line) {
        std::string output = line;
        if (options.encoding == "CP1251") {
            output = convertUtf8ToCp1251(line);
        }
        file << output << "\n";
    };

    // Заголовок
    if (options.include_header) {
        std::ostringstream header;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) header << options.delimiter;
            header << (options.use_russian_headers
                ? getFieldNameRu(fields[i])
                : getFieldNameEn(fields[i]));
        }
        writeLine(header.str());
    }

    // Данные
    for (const auto& pt : result.points) {
        std::ostringstream row;

        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) row << options.delimiter;

            switch (fields[i]) {
                case ExportField::Depth:
                    row << formatDouble(pt.depth.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::Inclination:
                    row << formatDouble(pt.inclination.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::MagneticAzimuth:
                    row << formatOptionalDegrees(pt.magnetic_azimuth, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::TrueAzimuth:
                    row << formatOptionalDegrees(pt.true_azimuth, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::X:
                    row << formatDouble(pt.x.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::Y:
                    row << formatDouble(pt.y.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::TVD:
                    row << formatDouble(pt.tvd.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::ABSG:
                    row << formatDouble(pt.absg.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::Shift:
                    row << formatDouble(pt.shift.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::DirectionAngle:
                    row << formatDouble(pt.direction_angle.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::Elongation:
                    row << formatDouble(pt.elongation.value, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::Intensity10m:
                    row << formatDouble(pt.intensity_10m, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::IntensityL:
                    row << formatDouble(pt.intensity_L, options.decimal_places, options.decimal_separator);
                    break;
                case ExportField::Rotation:
                    if (pt.rotation.has_value()) {
                        row << formatDouble(*pt.rotation, options.decimal_places, options.decimal_separator);
                    }
                    break;
                case ExportField::ROP:
                    if (pt.rop.has_value()) {
                        row << formatDouble(*pt.rop, options.decimal_places, options.decimal_separator);
                    }
                    break;
                case ExportField::ErrorX:
                    row << formatDouble(pt.error_x.value, options.decimal_places + 1, options.decimal_separator);
                    break;
                case ExportField::ErrorY:
                    row << formatDouble(pt.error_y.value, options.decimal_places + 1, options.decimal_separator);
                    break;
                case ExportField::ErrorABSG:
                    row << formatDouble(pt.error_absg.value, options.decimal_places + 1, options.decimal_separator);
                    break;
                case ExportField::Marker:
                    if (pt.marker.has_value()) {
                        row << *pt.marker;
                    }
                    break;
            }
        }

        writeLine(row.str());
    }

    file.close();

    // Атомарное переименование
    try {
        std::filesystem::rename(temp_path, path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::filesystem::remove(temp_path);
        throw CsvWriteError("Ошибка сохранения файла: " + std::string(e.what()));
    }
}

void writeCsvMeasurements(
    const IntervalData& data,
    const std::filesystem::path& path,
    const CsvExportOptions& options
) {
    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    std::ofstream file(temp_path);
    if (!file) {
        throw CsvWriteError("Не удалось создать файл: " + path.string());
    }

    auto writeLine = [&](const std::string& line) {
        std::string output = line;
        if (options.encoding == "CP1251") {
            output = convertUtf8ToCp1251(line);
        }
        file << output << "\n";
    };

    // Заголовок
    if (options.include_header) {
        std::ostringstream header;
        if (options.use_russian_headers) {
            header << "Глубина" << options.delimiter
                   << "Угол" << options.delimiter
                   << "Азимут_магн" << options.delimiter
                   << "Азимут_ист" << options.delimiter
                   << "ВРАЩ" << options.delimiter
                   << "СКОР" << options.delimiter
                   << "Метка";
        } else {
            header << "Depth" << options.delimiter
                   << "Inc" << options.delimiter
                   << "Azim_Mag" << options.delimiter
                   << "Azim_True" << options.delimiter
                   << "Rot" << options.delimiter
                   << "ROP" << options.delimiter
                   << "Marker";
        }
        writeLine(header.str());
    }

    // Данные
    for (const auto& m : data.measurements) {
        std::ostringstream row;

        row << formatDouble(m.depth.value, options.decimal_places, options.decimal_separator)
            << options.delimiter
            << formatDouble(m.inclination.value, options.decimal_places, options.decimal_separator)
            << options.delimiter
            << formatOptionalDegrees(m.magnetic_azimuth, options.decimal_places, options.decimal_separator)
            << options.delimiter
            << formatOptionalDegrees(m.true_azimuth, options.decimal_places, options.decimal_separator)
            << options.delimiter
            << (m.rotation.has_value() ? formatDouble(*m.rotation, options.decimal_places, options.decimal_separator) : "")
            << options.delimiter;

        if (m.rop.has_value()) {
            row << formatDouble(*m.rop, options.decimal_places, options.decimal_separator);
        }
        row << options.delimiter;

        if (m.marker.has_value()) {
            row << *m.marker;
        }

        writeLine(row.str());
    }

    file.close();

    try {
        std::filesystem::rename(temp_path, path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::filesystem::remove(temp_path);
        throw CsvWriteError("Ошибка сохранения файла: " + std::string(e.what()));
    }
}

} // namespace incline::io

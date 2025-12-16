/**
 * @file las_reader.cpp
 * @brief Реализация импорта данных из LAS 2.0
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "las_reader.hpp"
#include "csv_reader.hpp"  // Для convertCp1251ToUtf8
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace incline::io {

namespace {

std::string trim(std::string_view str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return std::string(str.substr(start, end - start));
}

std::string toUpper(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

// Альтернативные мнемоники для кривых
const std::set<std::string> kDepthMnemonics = {"DEPT", "MD", "DEPTH", "TVD"};
const std::set<std::string> kInclinationMnemonics = {"INCL", "INC", "DEVI", "DEVIATION", "ANGLE"};
const std::set<std::string> kAzimuthMnemonics = {"AZIM", "AZI", "HAZI", "AZIMUTH", "AZ"};
const std::set<std::string> kTrueAzimuthMnemonics = {"AZIT", "TAZI", "DAZI", "AZ_TRUE", "TRUE_AZ"};

enum class LasSection {
    None,
    Version,
    Well,
    Curve,
    Parameter,
    Other,
    Ascii
};

struct LasLine {
    std::string mnemonic;
    std::string unit;
    std::string value;
    std::string description;
};

LasLine parseLasLine(std::string_view line) {
    LasLine result;

    // Формат: MNEM.UNIT   VALUE : DESCRIPTION
    size_t dot_pos = line.find('.');
    if (dot_pos == std::string::npos) {
        return result;
    }

    result.mnemonic = trim(line.substr(0, dot_pos));

    // Ищем конец единиц измерения (пробел или начало значения)
    size_t unit_end = dot_pos + 1;
    while (unit_end < line.size() &&
           !std::isspace(static_cast<unsigned char>(line[unit_end])) &&
           line[unit_end] != ':') {
        ++unit_end;
    }
    result.unit = trim(line.substr(dot_pos + 1, unit_end - dot_pos - 1));

    // Ищем двоеточие
    size_t colon_pos = line.find(':', unit_end);
    if (colon_pos != std::string::npos) {
        result.value = trim(line.substr(unit_end, colon_pos - unit_end));
        result.description = trim(line.substr(colon_pos + 1));
    } else {
        result.value = trim(line.substr(unit_end));
    }

    return result;
}

std::vector<double> parseDataLine(std::string_view line) {
    std::vector<double> values;
    std::string line_str{line};
    std::istringstream ss{line_str};
    double val;
    while (ss >> val) {
        values.push_back(val);
    }
    return values;
}

} // anonymous namespace

bool canReadLas(const std::filesystem::path& path) noexcept {
    try {
        if (!std::filesystem::exists(path)) {
            return false;
        }

        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (ext != ".las") {
            return false;
        }

        // Проверяем начало файла
        std::ifstream file(path);
        if (!file) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            // LAS файл должен начинаться с ~V
            std::string upper = toUpper(line);
            return upper.find("~V") == 0;
        }

        return false;
    } catch (...) {
        return false;
    }
}

std::vector<LasCurveInfo> getLasCurves(const std::filesystem::path& path) {
    std::vector<LasCurveInfo> curves;

    std::ifstream file(path);
    if (!file) {
        return curves;
    }

    LasSection section = LasSection::None;
    std::string line;
    size_t curve_index = 0;

    while (std::getline(file, line)) {
        // Конвертируем из CP1251 если нужно
        if (!line.empty() && static_cast<unsigned char>(line[0]) >= 0x80) {
            line = convertCp1251ToUtf8(line);
        }

        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Проверяем секции
        if (line[0] == '~') {
            auto section_char = static_cast<char>(std::toupper(static_cast<unsigned char>(line[1])));
            switch (section_char) {
                case 'C': section = LasSection::Curve; continue;
                case 'A': section = LasSection::Ascii; continue;
                default: section = LasSection::Other; continue;
            }
        }

        if (section == LasSection::Curve) {
            auto parsed = parseLasLine(line);
            if (!parsed.mnemonic.empty()) {
                LasCurveInfo info;
                info.mnemonic = toUpper(parsed.mnemonic);
                info.unit = parsed.unit;
                info.description = parsed.description;
                info.column_index = curve_index++;
                curves.push_back(info);
            }
        }

        if (section == LasSection::Ascii) {
            break;  // Достаточно информации
        }
    }

    return curves;
}

LasReadResult readLas(
    const std::filesystem::path& path,
    const LasReadOptions& options
) {
    LasReadResult result;
    result.null_value = options.null_value;

    std::ifstream file(path);
    if (!file) {
        throw LasReadError("Не удалось открыть файл: " + path.string());
    }

    LasSection section = LasSection::None;
    std::string line;
    size_t line_num = 0;
    size_t curve_index = 0;

    // Индексы нужных кривых
    int depth_idx = -1;
    int inc_idx = -1;
    int az_idx = -1;
    int az_true_idx = -1;

    while (std::getline(file, line)) {
        ++line_num;

        // Конвертируем из CP1251 если нужно
        if (!line.empty() && static_cast<unsigned char>(line[0]) >= 0x80) {
            line = convertCp1251ToUtf8(line);
        }

        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Проверяем секции
        if (line[0] == '~') {
            auto section_char = static_cast<char>(std::toupper(static_cast<unsigned char>(line[1])));
            switch (section_char) {
                case 'V': section = LasSection::Version; continue;
                case 'W': section = LasSection::Well; continue;
                case 'C': section = LasSection::Curve; continue;
                case 'P': section = LasSection::Parameter; continue;
                case 'A': section = LasSection::Ascii; continue;
                default: section = LasSection::Other; continue;
            }
        }

        switch (section) {
            case LasSection::Version: {
                auto parsed = parseLasLine(line);
                std::string mnem = toUpper(parsed.mnemonic);
                if (mnem == "VERS") {
                    result.version = parsed.value;
                } else if (mnem == "NULL") {
                    try {
                        result.null_value = std::stod(parsed.value);
                    } catch (...) {}
                }
                break;
            }

            case LasSection::Well: {
                auto parsed = parseLasLine(line);
                std::string mnem = toUpper(parsed.mnemonic);
                result.well_info[mnem] = parsed.value;

                // Заполняем метаданные
                if (mnem == "WELL") {
                    result.data.well = parsed.value;
                } else if (mnem == "FLD") {
                    result.data.field = parsed.value;
                } else if (mnem == "LOC") {
                    result.data.cluster = parsed.value;
                } else if (mnem == "COMP") {
                    result.data.contractor = parsed.value;
                } else if (mnem == "SRVC") {
                    if (result.data.contractor.empty()) {
                        result.data.contractor = parsed.value;
                    }
                } else if (mnem == "DATE") {
                    result.data.study_date = parsed.value;
                } else if (mnem == "STRT") {
                    try {
                        result.data.interval_start = Meters{std::stod(parsed.value)};
                    } catch (...) {}
                } else if (mnem == "STOP") {
                    try {
                        result.data.interval_end = Meters{std::stod(parsed.value)};
                    } catch (...) {}
                } else if (mnem == "NULL") {
                    try {
                        result.null_value = std::stod(parsed.value);
                    } catch (...) {}
                }
                break;
            }

            case LasSection::Curve: {
                auto parsed = parseLasLine(line);
                if (parsed.mnemonic.empty()) break;

                LasCurveInfo info;
                info.mnemonic = toUpper(parsed.mnemonic);
                info.unit = parsed.unit;
                info.description = parsed.description;
                info.column_index = curve_index;

                // Определяем тип кривой
                if (options.auto_detect_curves) {
                    if (kDepthMnemonics.count(info.mnemonic) && depth_idx < 0) {
                        depth_idx = static_cast<int>(curve_index);
                    } else if (kInclinationMnemonics.count(info.mnemonic) && inc_idx < 0) {
                        inc_idx = static_cast<int>(curve_index);
                    } else if (kTrueAzimuthMnemonics.count(info.mnemonic) && az_true_idx < 0) {
                        az_true_idx = static_cast<int>(curve_index);
                    } else if (kAzimuthMnemonics.count(info.mnemonic) && az_idx < 0) {
                        az_idx = static_cast<int>(curve_index);
                    }
                } else {
                    if (info.mnemonic == toUpper(options.mnemonics.depth)) {
                        depth_idx = static_cast<int>(curve_index);
                    } else if (info.mnemonic == toUpper(options.mnemonics.inclination)) {
                        inc_idx = static_cast<int>(curve_index);
                    } else if (info.mnemonic == toUpper(options.mnemonics.azimuth)) {
                        az_idx = static_cast<int>(curve_index);
                    } else if (info.mnemonic == toUpper(options.mnemonics.true_azimuth)) {
                        az_true_idx = static_cast<int>(curve_index);
                    }
                }

                result.curves.push_back(info);
                ++curve_index;
                break;
            }

            case LasSection::Ascii: {
                auto values = parseDataLine(line);
                if (values.empty()) continue;

                // Проверяем минимальные требования
                if (depth_idx < 0 || inc_idx < 0) {
                    throw LasReadError(
                        "Не найдены обязательные кривые глубины и зенитного угла",
                        line_num
                    );
                }

                if (static_cast<size_t>(depth_idx) >= values.size() ||
                    static_cast<size_t>(inc_idx) >= values.size()) {
                    continue;  // Пропускаем некорректные строки
                }

                MeasurementPoint point;

                // Глубина
                double depth = values[static_cast<size_t>(depth_idx)];
                if (isLasNull(depth, result.null_value)) continue;
                point.depth = Meters{depth};

                // Зенитный угол
                double inc = values[static_cast<size_t>(inc_idx)];
                if (isLasNull(inc, result.null_value)) {
                    inc = 0.0;  // Предполагаем вертикаль
                }
                point.inclination = Degrees{inc};

                // Магнитный азимут
                if (az_idx >= 0 && static_cast<size_t>(az_idx) < values.size()) {
                    double az = values[static_cast<size_t>(az_idx)];
                    if (!isLasNull(az, result.null_value)) {
                        point.magnetic_azimuth = Degrees{az};
                    }
                }

                // Истинный азимут
                if (az_true_idx >= 0 && static_cast<size_t>(az_true_idx) < values.size()) {
                    double az = values[static_cast<size_t>(az_true_idx)];
                    if (!isLasNull(az, result.null_value)) {
                        point.true_azimuth = Degrees{az};
                    }
                }

                result.data.measurements.push_back(point);
                break;
            }

            default:
                break;
        }
    }

    if (result.data.measurements.empty()) {
        throw LasReadError("Файл не содержит данных замеров");
    }

    // Заполняем имя скважины из имени файла если пусто
    if (result.data.well.empty()) {
        result.data.well = path.stem().string();
    }

    return result;
}

IntervalData readLasMeasurements(
    const std::filesystem::path& path,
    const LasReadOptions& options
) {
    return readLas(path, options).data;
}

} // namespace incline::io

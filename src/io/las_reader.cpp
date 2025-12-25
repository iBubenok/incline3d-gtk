/**
 * @file las_reader.cpp
 * @brief Реализация импорта данных из LAS 2.0
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "las_reader.hpp"
#include "csv_reader.hpp"  // Для convertCp1251ToUtf8
#include "text_utils.hpp"
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

std::string normalizeMnemonic(std::string_view raw) {
    auto upper = utf8ToUpper(trim(raw));
    std::string cleaned;
    cleaned.reserve(upper.size());
    for (size_t i = 0; i < upper.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(upper[i]);
        if (c < 0x80) {
            if (std::isalnum(c)) {
                cleaned.push_back(static_cast<char>(c));
            } else if (c == '_' || c == '-' || c == ' ') {
                cleaned.push_back('_');
            }
        } else {
            cleaned.push_back(static_cast<char>(c));
        }
    }
    while (!cleaned.empty() && cleaned.back() == '_') {
        cleaned.pop_back();
    }
    return cleaned;
}

std::string normalizeUnit(std::string_view raw) {
    return utf8ToUpper(trim(raw));
}

bool unitLooksLikeDepth(const std::string& unit_upper) {
    return unit_upper.find("M") != std::string::npos ||
           unit_upper.find("FT") != std::string::npos;
}

bool unitLooksLikeAngle(const std::string& unit_upper) {
    return unit_upper.find("DEG") != std::string::npos ||
           unit_upper.find("GRAD") != std::string::npos ||
           unit_upper.find("\xD0\x93\xD0\xA0\xD0\x90\xD0\x94") != std::string::npos; // "ГРАД" в UTF-8
}

// Альтернативные мнемоники для кривых
const std::set<std::string> kDepthMnemonics = {
    "DEPT", "DEPTH", "MD", "MEASUREDDEPTH", "DEPTHMD", "DEPTMD",
    "DEPT_M", "DEPTH_M", "TVD", "GLUBINA", "ГЛУБИНА", "ГЛУБ"
};
const std::set<std::string> kInclinationMnemonics = {
    "INCL", "INC", "INCLINATION", "DEV", "DEVI", "DEVIATION", "ANGLE",
    "ZENIT", "ZENITH", "UGOL", "УГОЛ", "ZEN"
};
const std::set<std::string> kAzimuthMnemonics = {
    "AZIM", "AZI", "HAZI", "AZIMUTH", "AZ", "MAGAZ", "AZM", "AZIMUT"
};
const std::set<std::string> kTrueAzimuthMnemonics = {
    "AZIT", "TAZI", "DAZI", "AZ_TRUE", "TRUE_AZ", "TRUEAZ", "AZTRUE", "AZT"
};

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
            std::string upper = utf8ToUpper(line);
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
                info.mnemonic = normalizeMnemonic(parsed.mnemonic);
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

LasCurveDetection detectLasCurves(
    const std::vector<LasCurveInfo>& curves,
    const LasReadOptions& options
) {
    LasCurveDetection detection;
    if (curves.empty()) {
        detection.diagnostics.push_back("Секция ~Curve отсутствует или не содержит кривых");
        return detection;
    }

    std::vector<std::string> normalized_mnemonics;
    std::vector<std::string> normalized_units;
    normalized_mnemonics.reserve(curves.size());
    normalized_units.reserve(curves.size());

    for (const auto& curve : curves) {
        normalized_mnemonics.push_back(normalizeMnemonic(curve.mnemonic));
        normalized_units.push_back(normalizeUnit(curve.unit));
    }

    auto matchByName = [&](const std::string& target) -> std::optional<int> {
        auto normalized = normalizeMnemonic(target);
        for (size_t i = 0; i < normalized_mnemonics.size(); ++i) {
            if (normalized_mnemonics[i] == normalized) {
                return static_cast<int>(i);
            }
        }
        return std::nullopt;
    };

    if (!options.auto_detect_curves) {
        detection.depth_index = matchByName(options.mnemonics.depth);
        detection.inclination_index = matchByName(options.mnemonics.inclination);
        detection.azimuth_index = matchByName(options.mnemonics.azimuth);
        detection.true_azimuth_index = matchByName(options.mnemonics.true_azimuth);

        if (!detection.depth_index.has_value()) {
            detection.diagnostics.push_back("Глубина не найдена по мнемонике " + options.mnemonics.depth);
        }
        if (!detection.inclination_index.has_value()) {
            detection.diagnostics.push_back("Зенитный угол не найден по мнемонике " + options.mnemonics.inclination);
        }
        return detection;
    }

    // Автоопределение по мнемонике
    for (size_t i = 0; i < normalized_mnemonics.size(); ++i) {
        const auto& mnem = normalized_mnemonics[i];
        if (!detection.depth_index.has_value() && kDepthMnemonics.count(mnem) > 0) {
            detection.depth_index = static_cast<int>(i);
            continue;
        }
        if (!detection.inclination_index.has_value() && kInclinationMnemonics.count(mnem) > 0) {
            detection.inclination_index = static_cast<int>(i);
            continue;
        }
        if (!detection.true_azimuth_index.has_value() && kTrueAzimuthMnemonics.count(mnem) > 0) {
            detection.true_azimuth_index = static_cast<int>(i);
            continue;
        }
        if (!detection.azimuth_index.has_value() && kAzimuthMnemonics.count(mnem) > 0) {
            detection.azimuth_index = static_cast<int>(i);
        }
    }

    // Дополнительные эвристики по единицам измерения
    if (!detection.depth_index.has_value()) {
        for (size_t i = 0; i < normalized_units.size(); ++i) {
            if (unitLooksLikeDepth(normalized_units[i])) {
                detection.depth_index = static_cast<int>(i);
                detection.diagnostics.push_back("Глубина выбрана по единицам измерения (" + normalized_units[i] + ")");
                break;
            }
        }
    }

    if (!detection.inclination_index.has_value()) {
        for (size_t i = 0; i < normalized_units.size(); ++i) {
            if (detection.depth_index.has_value() && static_cast<int>(i) == *detection.depth_index) {
                continue;
            }
            if (unitLooksLikeAngle(normalized_units[i])) {
                detection.inclination_index = static_cast<int>(i);
                detection.diagnostics.push_back("Зенитный угол выбран по единицам измерения (" + normalized_units[i] + ")");
                break;
            }
        }
    }

    if (!detection.azimuth_index.has_value()) {
        for (size_t i = 0; i < normalized_units.size(); ++i) {
            if (detection.depth_index.has_value() && static_cast<int>(i) == *detection.depth_index) continue;
            if (detection.inclination_index.has_value() && static_cast<int>(i) == *detection.inclination_index) continue;
            if (unitLooksLikeAngle(normalized_units[i]) || kAzimuthMnemonics.count(normalized_mnemonics[i]) > 0) {
                detection.azimuth_index = static_cast<int>(i);
                detection.diagnostics.push_back("Магнитный азимут выбран по единицам/мнемонике (" + normalized_mnemonics[i] + ")");
                break;
            }
        }
    }

    // Жёсткий fallback: первая колонка = глубина, вторая = зенит
    if (!detection.depth_index.has_value()) {
        detection.depth_index = 0;
        detection.diagnostics.push_back("Глубина не найдена по мнемоникам — использована первая колонка данных");
    }
    if (!detection.inclination_index.has_value()) {
        if (normalized_mnemonics.size() >= 2) {
            detection.inclination_index = 1;
            detection.diagnostics.push_back("Зенитный угол не найден по мнемоникам — использована вторая колонка данных");
        }
    }

    return detection;
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
    LasCurveDetection curve_detection;
    bool mapping_resolved = false;

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
                std::string mnem = normalizeMnemonic(parsed.mnemonic);
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
                std::string mnem = normalizeMnemonic(parsed.mnemonic);
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
                info.mnemonic = normalizeMnemonic(parsed.mnemonic);
                info.unit = parsed.unit;
                info.description = parsed.description;
                info.column_index = curve_index;

                result.curves.push_back(info);
                ++curve_index;
                break;
            }

            case LasSection::Ascii: {
                if (!mapping_resolved) {
                    curve_detection = detectLasCurves(result.curves, options);
                    depth_idx = curve_detection.depth_index.value_or(-1);
                    inc_idx = curve_detection.inclination_index.value_or(-1);
                    az_idx = curve_detection.azimuth_index.value_or(-1);
                    az_true_idx = curve_detection.true_azimuth_index.value_or(-1);
                    mapping_resolved = true;
                }

                auto values = parseDataLine(line);
                if (values.empty()) continue;

                // Проверяем минимальные требования
                if (depth_idx < 0 || inc_idx < 0) {
                    std::ostringstream oss;
                    oss << "Не найдены обязательные кривые глубины и зенитного угла.";
                    if (!result.curves.empty()) {
                        oss << " Обнаружены кривые: ";
                        for (size_t i = 0; i < result.curves.size(); ++i) {
                            if (i > 0) oss << ", ";
                            oss << result.curves[i].mnemonic;
                        }
                    }
                    if (!curve_detection.diagnostics.empty()) {
                        oss << " Детали: ";
                        for (size_t i = 0; i < curve_detection.diagnostics.size(); ++i) {
                            if (i > 0) oss << " ";
                            oss << curve_detection.diagnostics[i];
                        }
                    } else {
                        oss << " Переименуйте кривые в DEPTH/MD/DEPT и INCL/INC/ZENIT или задайте маппинг вручную.";
                    }
                    throw LasReadError(oss.str(), line_num);
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

/**
 * @file csv_reader.cpp
 * @brief Реализация импорта данных из CSV
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "csv_reader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <array>
#include <unordered_map>

namespace incline::io {

namespace {

// Словарь алиасов для автоопределения колонок
const std::unordered_map<std::string, std::string> kFieldAliases = {
    // Глубина
    {"глубина", "depth"}, {"depth", "depth"}, {"md", "depth"},
    {"dept", "depth"}, {"гл", "depth"}, {"глуб", "depth"},

    // Зенитный угол
    {"угол", "inclination"}, {"inclination", "inclination"},
    {"inc", "inclination"}, {"incl", "inclination"},
    {"зенит", "inclination"}, {"зенитный", "inclination"},
    {"devi", "inclination"},

    // Магнитный азимут
    {"азимут", "magnetic_azimuth"}, {"azimuth", "magnetic_azimuth"},
    {"az", "magnetic_azimuth"}, {"azi", "magnetic_azimuth"},
    {"azim", "magnetic_azimuth"}, {"азимут_магн", "magnetic_azimuth"},
    {"hazi", "magnetic_azimuth"},

    // Истинный азимут
    {"азимут_истинный", "true_azimuth"}, {"true_azimuth", "true_azimuth"},
    {"az_true", "true_azimuth"}, {"azit", "true_azimuth"},
    {"tazi", "true_azimuth"}, {"dazi", "true_azimuth"},
    {"азимут_геогр", "true_azimuth"},

    // Положение отклонителя
    {"вращ", "rotation"}, {"rotation", "rotation"},
    {"rot", "rotation"}, {"tf", "rotation"},
    {"toolface", "rotation"},

    // Скорость проходки
    {"скор", "rop"}, {"rop", "rop"},
    {"rate", "rop"}, {"скорость", "rop"},

    // Маркер
    {"метка", "marker"}, {"marker", "marker"},
    {"mark", "marker"}, {"comment", "marker"}
};

std::string toLower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        if (static_cast<unsigned char>(c) < 128) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            // Для кириллицы в UTF-8
            result += c;
        }
    }
    return result;
}

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

std::vector<std::string> splitLine(std::string_view line, char delimiter) {
    std::vector<std::string> result;
    std::string current;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == delimiter && !in_quotes) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }

    result.push_back(trim(current));
    return result;
}

double parseDouble(const std::string& str, char decimal_sep) {
    if (str.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    std::string normalized = str;
    if (decimal_sep == ',') {
        std::replace(normalized.begin(), normalized.end(), ',', '.');
    }

    try {
        size_t pos = 0;
        double value = std::stod(normalized, &pos);
        // Проверяем, что вся строка была обработана
        while (pos < normalized.size() && std::isspace(static_cast<unsigned char>(normalized[pos]))) {
            ++pos;
        }
        if (pos != normalized.size()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return value;
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

std::string identifyField(const std::string& header) {
    std::string lower = toLower(trim(header));

    // Удаляем спецсимволы и лишние пробелы
    std::string clean;
    for (char c : lower) {
        if (std::isalnum(static_cast<unsigned char>(c)) || static_cast<unsigned char>(c) > 127) {
            clean += c;
        } else if (c == '_' || c == '-') {
            clean += '_';
        }
    }

    auto it = kFieldAliases.find(clean);
    if (it != kFieldAliases.end()) {
        return it->second;
    }

    // Пробуем без подчёркиваний
    std::string no_underscore;
    for (char c : clean) {
        if (c != '_') no_underscore += c;
    }
    it = kFieldAliases.find(no_underscore);
    if (it != kFieldAliases.end()) {
        return it->second;
    }

    return "";
}

char detectDelimiter(const std::vector<std::string>& lines) {
    std::array<char, 4> candidates = {';', ',', '\t', '|'};
    std::array<int, 4> scores = {0, 0, 0, 0};
    std::array<int, 4> counts = {0, 0, 0, 0};

    // Подсчитываем количество каждого разделителя
    for (const auto& line : lines) {
        for (size_t i = 0; i < candidates.size(); ++i) {
            int count = static_cast<int>(std::count(line.begin(), line.end(), candidates[i]));
            if (count > 0) {
                ++scores[i];
                counts[i] += count;
            }
        }
    }

    // Проверяем консистентность (одинаковое количество в каждой строке)
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (scores[i] == 0) continue;

        int expected_count = 0;
        bool consistent = true;
        for (const auto& line : lines) {
            int count = static_cast<int>(std::count(line.begin(), line.end(), candidates[i]));
            if (expected_count == 0) {
                expected_count = count;
            } else if (count != expected_count && count > 0) {
                consistent = false;
                break;
            }
        }

        if (consistent && expected_count > 0) {
            return candidates[i];
        }
    }

    // Возвращаем самый частый
    size_t best = 0;
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (counts[i] > counts[best]) {
            best = i;
        }
    }

    return candidates[best];
}

bool looksLikeHeader(const std::vector<std::string>& fields) {
    int text_count = 0;
    int number_count = 0;

    for (const auto& field : fields) {
        if (field.empty()) continue;

        // Пробуем распарсить как число
        try {
            std::stod(field);
            ++number_count;
        } catch (...) {
            ++text_count;
        }
    }

    // Если большинство полей — текст, это вероятно заголовок
    return text_count > number_count;
}

} // anonymous namespace

std::string convertCp1251ToUtf8(const std::string& input) {
    std::string result;
    result.reserve(input.size() * 2);

    // Таблица конвертации CP1251 -> UTF-8 для символов 0x80-0xFF
    static const char* cp1251_to_utf8[] = {
        "\xD0\x82", "\xD0\x83", "\xE2\x80\x9A", "\xD1\x93", "\xE2\x80\x9E", "\xE2\x80\xA6", "\xE2\x80\xA0", "\xE2\x80\xA1",
        "\xE2\x82\xAC", "\xE2\x80\xB0", "\xD0\x89", "\xE2\x80\xB9", "\xD0\x8A", "\xD0\x8C", "\xD0\x8B", "\xD0\x8F",
        "\xD1\x92", "\xE2\x80\x98", "\xE2\x80\x99", "\xE2\x80\x9C", "\xE2\x80\x9D", "\xE2\x80\xA2", "\xE2\x80\x93", "\xE2\x80\x94",
        nullptr, "\xE2\x84\xA2", "\xD1\x99", "\xE2\x80\xBA", "\xD1\x9A", "\xD1\x9C", "\xD1\x9B", "\xD1\x9F",
        "\xC2\xA0", "\xD0\x8E", "\xD1\x9E", "\xD0\x88", "\xC2\xA4", "\xD2\x90", "\xC2\xA6", "\xC2\xA7",
        "\xD0\x81", "\xC2\xA9", "\xD0\x84", "\xC2\xAB", "\xC2\xAC", "\xC2\xAD", "\xC2\xAE", "\xD0\x87",
        "\xC2\xB0", "\xC2\xB1", "\xD0\x86", "\xD1\x96", "\xD2\x91", "\xC2\xB5", "\xC2\xB6", "\xC2\xB7",
        "\xD1\x91", "\xE2\x84\x96", "\xD1\x94", "\xC2\xBB", "\xD1\x98", "\xD0\x85", "\xD1\x95", "\xD1\x97",
        "\xD0\x90", "\xD0\x91", "\xD0\x92", "\xD0\x93", "\xD0\x94", "\xD0\x95", "\xD0\x96", "\xD0\x97",
        "\xD0\x98", "\xD0\x99", "\xD0\x9A", "\xD0\x9B", "\xD0\x9C", "\xD0\x9D", "\xD0\x9E", "\xD0\x9F",
        "\xD0\xA0", "\xD0\xA1", "\xD0\xA2", "\xD0\xA3", "\xD0\xA4", "\xD0\xA5", "\xD0\xA6", "\xD0\xA7",
        "\xD0\xA8", "\xD0\xA9", "\xD0\xAA", "\xD0\xAB", "\xD0\xAC", "\xD0\xAD", "\xD0\xAE", "\xD0\xAF",
        "\xD0\xB0", "\xD0\xB1", "\xD0\xB2", "\xD0\xB3", "\xD0\xB4", "\xD0\xB5", "\xD0\xB6", "\xD0\xB7",
        "\xD0\xB8", "\xD0\xB9", "\xD0\xBA", "\xD0\xBB", "\xD0\xBC", "\xD0\xBD", "\xD0\xBE", "\xD0\xBF",
        "\xD1\x80", "\xD1\x81", "\xD1\x82", "\xD1\x83", "\xD1\x84", "\xD1\x85", "\xD1\x86", "\xD1\x87",
        "\xD1\x88", "\xD1\x89", "\xD1\x8A", "\xD1\x8B", "\xD1\x8C", "\xD1\x8D", "\xD1\x8E", "\xD1\x8F"
    };

    for (unsigned char c : input) {
        if (c < 0x80) {
            result += static_cast<char>(c);
        } else {
            const char* utf8 = cp1251_to_utf8[c - 0x80];
            if (utf8) {
                result += utf8;
            } else {
                result += '?';
            }
        }
    }

    return result;
}

std::string detectEncoding(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "UNKNOWN";
    }

    // Читаем первые 1024 байта
    std::vector<unsigned char> buffer(1024);
    file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    size_t bytes_read = file.gcount();
    buffer.resize(bytes_read);

    // Проверка BOM
    if (bytes_read >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF) {
        return "UTF-8";
    }

    // Подсчёт признаков UTF-8 и CP1251
    int utf8_sequences = 0;
    int cp1251_chars = 0;
    int invalid_utf8 = 0;

    for (size_t i = 0; i < bytes_read; ++i) {
        unsigned char c = buffer[i];

        if (c < 0x80) {
            // ASCII
            continue;
        }

        // Проверяем UTF-8 последовательности
        if ((c & 0xE0) == 0xC0 && i + 1 < bytes_read) {
            // 2-байтовая последовательность
            if ((buffer[i + 1] & 0xC0) == 0x80) {
                ++utf8_sequences;
                ++i;
                continue;
            }
        } else if ((c & 0xF0) == 0xE0 && i + 2 < bytes_read) {
            // 3-байтовая последовательность
            if ((buffer[i + 1] & 0xC0) == 0x80 && (buffer[i + 2] & 0xC0) == 0x80) {
                ++utf8_sequences;
                i += 2;
                continue;
            }
        }

        // Если не UTF-8, проверяем диапазон CP1251 кириллицы
        if (c >= 0xC0 && c <= 0xFF) {
            ++cp1251_chars;
        } else if (c >= 0x80 && c <= 0xBF) {
            ++invalid_utf8;
        }
    }

    if (utf8_sequences > 0 && invalid_utf8 == 0) {
        return "UTF-8";
    }

    if (cp1251_chars > utf8_sequences) {
        return "CP1251";
    }

    return "UTF-8";  // По умолчанию
}

CsvDetectionResult detectCsvFormat(const std::filesystem::path& path) {
    CsvDetectionResult result;

    std::string encoding = detectEncoding(path);
    std::ifstream file(path);
    if (!file) {
        return result;
    }

    // Читаем первые 20 строк
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line) && lines.size() < 20) {
        if (encoding == "CP1251") {
            line = convertCp1251ToUtf8(line);
        }
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    if (lines.empty()) {
        return result;
    }

    // Определяем разделитель
    result.detected_delimiter = detectDelimiter(lines);

    // Разбиваем первую строку
    auto first_fields = splitLine(lines[0], result.detected_delimiter);
    result.column_count = first_fields.size();

    // Определяем наличие заголовка
    result.has_header = looksLikeHeader(first_fields);

    if (result.has_header) {
        result.header_names = first_fields;

        // Автоматический маппинг полей
        for (size_t i = 0; i < first_fields.size(); ++i) {
            std::string field_type = identifyField(first_fields[i]);
            if (field_type == "depth") {
                result.suggested_mapping.depth_column = i;
            } else if (field_type == "inclination") {
                result.suggested_mapping.inclination_column = i;
            } else if (field_type == "magnetic_azimuth") {
                result.suggested_mapping.magnetic_azimuth_column = i;
            } else if (field_type == "true_azimuth") {
                result.suggested_mapping.true_azimuth_column = i;
            } else if (field_type == "rotation") {
                result.suggested_mapping.rotation_column = i;
            } else if (field_type == "rop") {
                result.suggested_mapping.rop_column = i;
            } else if (field_type == "marker") {
                result.suggested_mapping.marker_column = i;
            }
        }
    } else {
        // Предполагаем стандартный порядок: глубина, угол, азимут
        if (result.column_count >= 1) result.suggested_mapping.depth_column = 0;
        if (result.column_count >= 2) result.suggested_mapping.inclination_column = 1;
        if (result.column_count >= 3) result.suggested_mapping.magnetic_azimuth_column = 2;
    }

    // Определяем десятичный разделитель
    size_t start_line = result.has_header ? 1 : 0;
    int dot_count = 0;
    int comma_count = 0;

    for (size_t i = start_line; i < lines.size() && i < 10; ++i) {
        auto fields = splitLine(lines[i], result.detected_delimiter);
        for (const auto& f : fields) {
            if (f.find('.') != std::string::npos) ++dot_count;
            if (f.find(',') != std::string::npos) ++comma_count;
        }
    }

    result.detected_decimal = (comma_count > dot_count && result.detected_delimiter != ',') ? ',' : '.';

    // Оценка уверенности
    result.confidence = 0.5;
    if (result.suggested_mapping.depth_column.has_value()) result.confidence += 0.15;
    if (result.suggested_mapping.inclination_column.has_value()) result.confidence += 0.15;
    if (result.suggested_mapping.magnetic_azimuth_column.has_value()) result.confidence += 0.1;
    if (result.has_header) result.confidence += 0.1;

    return result;
}

bool canReadCsv(const std::filesystem::path& path) noexcept {
    try {
        if (!std::filesystem::exists(path)) {
            return false;
        }

        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });

        return ext == ".csv" || ext == ".txt";
    } catch (...) {
        return false;
    }
}

IntervalData readCsvMeasurements(
    const std::filesystem::path& path,
    const CsvReadOptions& options
) {
    IntervalData data;

    // Определяем кодировку
    std::string encoding = options.encoding;
    if (encoding == "AUTO" || encoding.empty()) {
        encoding = detectEncoding(path);
    }

    std::ifstream file(path);
    if (!file) {
        throw CsvReadError("Не удалось открыть файл: " + path.string());
    }

    // Получаем маппинг (или автоопределяем)
    CsvFieldMapping mapping = options.mapping;
    if (!mapping.isValid()) {
        auto detection = detectCsvFormat(path);
        mapping = detection.suggested_mapping;

        if (!mapping.isValid()) {
            throw CsvReadError("Не удалось определить колонки глубины и угла");
        }
    }

    std::string line;
    size_t line_num = 0;

    // Пропускаем начальные строки
    while (line_num < options.skip_lines && std::getline(file, line)) {
        ++line_num;
    }

    // Пропускаем заголовок
    if (options.has_header && std::getline(file, line)) {
        ++line_num;
    }

    // Чтение данных
    while (std::getline(file, line)) {
        ++line_num;

        if (encoding == "CP1251") {
            line = convertCp1251ToUtf8(line);
        }

        line = trim(line);
        if (line.empty()) continue;

        auto fields = splitLine(line, options.delimiter);

        // Пропускаем строки с недостаточным количеством полей
        size_t required_cols = std::max(
            mapping.depth_column.value_or(0),
            mapping.inclination_column.value_or(0)
        ) + 1;

        if (fields.size() < required_cols) {
            continue;  // Пропускаем некорректные строки
        }

        MeasurementPoint point;

        // Глубина
        double depth = parseDouble(fields[*mapping.depth_column], options.decimal_separator);
        if (std::isnan(depth)) {
            throw CsvReadError(
                "Некорректное значение глубины: " + fields[*mapping.depth_column],
                line_num
            );
        }
        point.depth = Meters{depth};

        // Зенитный угол
        double inc = parseDouble(fields[*mapping.inclination_column], options.decimal_separator);
        if (std::isnan(inc)) {
            throw CsvReadError(
                "Некорректное значение угла: " + fields[*mapping.inclination_column],
                line_num
            );
        }
        point.inclination = Degrees{inc};

        // Магнитный азимут (опционально)
        if (mapping.magnetic_azimuth_column.has_value() &&
            *mapping.magnetic_azimuth_column < fields.size()) {
            double az = parseDouble(fields[*mapping.magnetic_azimuth_column], options.decimal_separator);
            if (!std::isnan(az)) {
                point.magnetic_azimuth = Degrees{az};
            }
        }

        // Истинный азимут (опционально)
        if (mapping.true_azimuth_column.has_value() &&
            *mapping.true_azimuth_column < fields.size()) {
            double az = parseDouble(fields[*mapping.true_azimuth_column], options.decimal_separator);
            if (!std::isnan(az)) {
                point.true_azimuth = Degrees{az};
            }
        }

        // Положение отклонителя (опционально)
        if (mapping.rotation_column.has_value() &&
            *mapping.rotation_column < fields.size()) {
            double rot = parseDouble(fields[*mapping.rotation_column], options.decimal_separator);
            if (!std::isnan(rot)) {
                point.rotation = rot;
            }
        }

        // Скорость проходки (опционально)
        if (mapping.rop_column.has_value() &&
            *mapping.rop_column < fields.size()) {
            double rop = parseDouble(fields[*mapping.rop_column], options.decimal_separator);
            if (!std::isnan(rop)) {
                point.rop = rop;
            }
        }

        // Маркер (опционально)
        if (mapping.marker_column.has_value() &&
            *mapping.marker_column < fields.size()) {
            std::string marker = trim(fields[*mapping.marker_column]);
            if (!marker.empty()) {
                point.marker = marker;
            }
        }

        data.measurements.push_back(point);
    }

    if (data.measurements.empty()) {
        throw CsvReadError("Файл не содержит данных замеров");
    }

    // Заполняем базовые метаданные из имени файла
    data.well = path.stem().string();

    return data;
}

} // namespace incline::io

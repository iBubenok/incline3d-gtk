/**
 * @file csv_reader.cpp
 * @brief Реализация импорта данных из CSV
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "csv_reader.hpp"
#include "text_utils.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <array>
#include <unordered_map>
#include <limits>

namespace incline::io {

namespace {

// Словарь алиасов для автоопределения колонок
const std::unordered_map<std::string, std::string> kFieldAliases = {
    // Глубина
    {"глубина", "depth"}, {"depth", "depth"}, {"md", "depth"},
    {"dept", "depth"}, {"measureddepth", "depth"}, {"dep", "depth"},
    {"гл", "depth"}, {"глуб", "depth"}, {"depthmd", "depth"}, {"deptm", "depth"},

    // Зенитный угол
    {"угол", "inclination"}, {"inclination", "inclination"},
    {"inc", "inclination"}, {"incl", "inclination"}, {"dev", "inclination"},
    {"devi", "inclination"}, {"deviation", "inclination"}, {"angle", "inclination"},
    {"zenith", "inclination"}, {"zenit", "inclination"}, {"зенит", "inclination"},
    {"зенитный", "inclination"}, {"инкл", "inclination"}, {"уголзенита", "inclination"},
    {"уголнаклона", "inclination"}, {"ugol", "inclination"},

    // Магнитный азимут
    {"азимут", "magnetic_azimuth"}, {"azimuth", "magnetic_azimuth"},
    {"az", "magnetic_azimuth"}, {"azi", "magnetic_azimuth"},
    {"azim", "magnetic_azimuth"}, {"азимут_магн", "magnetic_azimuth"},
    {"hazi", "magnetic_azimuth"}, {"magaz", "magnetic_azimuth"},

    // Истинный азимут
    {"азимут_истинный", "true_azimuth"}, {"true_azimuth", "true_azimuth"},
    {"az_true", "true_azimuth"}, {"azit", "true_azimuth"},
    {"tazi", "true_azimuth"}, {"dazi", "true_azimuth"},
    {"азимут_геогр", "true_azimuth"}, {"aztrue", "true_azimuth"},

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

std::string stripBom(std::string_view str) {
    if (str.size() >= 3 &&
        static_cast<unsigned char>(str[0]) == 0xEF &&
        static_cast<unsigned char>(str[1]) == 0xBB &&
        static_cast<unsigned char>(str[2]) == 0xBF) {
        return std::string(str.substr(3));
    }
    return std::string(str);
}

std::string normalizeHeaderToken(std::string_view value) {
    // Удаляем BOM и крайние пробелы
    auto cleaned = trim(stripBom(value));
    auto lowered = utf8ToLower(cleaned);

    std::string normalized;
    normalized.reserve(lowered.size());
    bool last_was_sep = false;

    for (size_t i = 0; i < lowered.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(lowered[i]);
        if (c < 0x80) {
            if (std::isalnum(c)) {
                normalized += static_cast<char>(c);
                last_was_sep = false;
            } else if (c == '_' || c == '-' || c == ' ' || c == '\t' || c == '/') {
                if (!last_was_sep && !normalized.empty()) {
                    normalized += '_';
                    last_was_sep = true;
                }
            }
        } else {
            normalized += static_cast<char>(c);
            last_was_sep = false;
        }
    }

    // Убираем завершающий разделитель
    while (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }
    return normalized;
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
    const std::string normalized = normalizeHeaderToken(header);
    if (normalized.empty()) {
        return "";
    }

    auto it = kFieldAliases.find(normalized);
    if (it != kFieldAliases.end()) {
        return it->second;
    }

    std::string compact;
    compact.reserve(normalized.size());
    for (char c : normalized) {
        if (c != '_') {
            compact += c;
        }
    }
    it = kFieldAliases.find(compact);
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

        // Пробуем распарсить как число (поддержка . и ,)
        double value = parseDouble(field, '.');
        if (std::isnan(value)) {
            value = parseDouble(field, ',');
        }
        if (std::isnan(value)) {
            ++text_count;
        } else {
            ++number_count;
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

    for (char ch : input) {
        auto c = static_cast<unsigned char>(ch);
        if (c < 0x80) {
            result += ch;
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
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    auto bytes_read = static_cast<size_t>(file.gcount());
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
        if (c >= 0xC0) {
            // Символ в диапазоне кириллицы CP1251 (0xC0-0xFF)
            ++cp1251_chars;
        } else if (c >= 0x80) {
            // Символ в диапазоне 0x80-0xBF (некорректный UTF-8)
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

    result.detected_encoding = detectEncoding(path);
    std::ifstream file(path);
    if (!file) {
        result.diagnostics.push_back("Не удалось открыть файл для автоопределения формата");
        return result;
    }

    // Читаем первые 50 строк (для статистики и заголовка)
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line) && lines.size() < 50) {
        if (result.detected_encoding == "CP1251") {
            line = convertCp1251ToUtf8(line);
        }
        auto cleaned = trim(stripBom(line));
        if (!cleaned.empty()) {
            lines.push_back(cleaned);
        }
    }

    if (lines.empty()) {
        result.diagnostics.push_back("Файл пуст или содержит только пустые строки");
        return result;
    }

    // Определяем разделитель
    result.detected_delimiter = detectDelimiter(lines);

    // Разбиваем строки
    std::vector<std::vector<std::string>> parsed;
    parsed.reserve(lines.size());
    size_t max_columns = 0;
    for (const auto& l : lines) {
        auto fields = splitLine(l, result.detected_delimiter);
        max_columns = std::max(max_columns, fields.size());
        parsed.push_back(fields);
    }
    result.column_count = max_columns;

    if (parsed.empty()) {
        result.diagnostics.push_back("Не удалось разобрать строки CSV");
        return result;
    }

    // Определяем наличие заголовка
    result.has_header = looksLikeHeader(parsed.front());

    if (result.has_header) {
        result.header_names = parsed.front();

        // Автоматический маппинг полей
        for (size_t i = 0; i < parsed.front().size(); ++i) {
            std::string field_type = identifyField(parsed.front()[i]);
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
        if (result.column_count >= 1) {
            result.suggested_mapping.depth_column = 0;
        }
        if (result.column_count >= 2) {
            result.suggested_mapping.inclination_column = 1;
        }
        if (result.column_count >= 3) {
            result.suggested_mapping.magnetic_azimuth_column = 2;
        }
    }

    // Определяем десятичный разделитель
    size_t start_line = result.has_header ? 1 : 0;
    int dot_count = 0;
    int comma_count = 0;

    for (size_t i = start_line; i < parsed.size() && i < 10; ++i) {
        for (const auto& f : parsed[i]) {
            if (f.find('.') != std::string::npos) {
                ++dot_count;
            }
            if (f.find(',') != std::string::npos) {
                ++comma_count;
            }
        }
    }

    result.detected_decimal = (comma_count > dot_count && result.detected_delimiter != ',') ? ',' : '.';

    struct ColumnStats {
        size_t numeric_count = 0;
        size_t total_count = 0;
        bool monotonic = true;
        std::optional<double> last_value;
        double min_value = std::numeric_limits<double>::infinity();
        double max_value = -std::numeric_limits<double>::infinity();
    };

    std::vector<ColumnStats> stats(max_columns);

    for (size_t row = start_line; row < parsed.size(); ++row) {
        const auto& fields = parsed[row];
        for (size_t col = 0; col < fields.size(); ++col) {
            auto& st = stats[col];
            st.total_count++;
            double value = parseDouble(fields[col], result.detected_decimal);
            if (std::isnan(value)) {
                char alt_decimal = (result.detected_decimal == ',') ? '.' : ',';
                value = parseDouble(fields[col], alt_decimal);
            }

            if (!std::isnan(value)) {
                st.numeric_count++;
                st.min_value = std::min(st.min_value, value);
                st.max_value = std::max(st.max_value, value);
                if (st.last_value.has_value() && value + 1e-9 < *st.last_value) {
                    st.monotonic = false;
                }
                st.last_value = value;
            }
        }
    }

    auto pickDepthByStats = [&]() -> std::optional<size_t> {
        size_t best = max_columns;
        size_t best_numeric = 0;
        for (size_t i = 0; i < stats.size(); ++i) {
            const auto& st = stats[i];
            if (st.numeric_count == 0) continue;
            bool candidate = st.monotonic && st.numeric_count >= 2;
            if (!candidate && best < max_columns) {
                continue;
            }
            if ((candidate && (best == max_columns || st.numeric_count > best_numeric)) ||
                (!candidate && best == max_columns)) {
                best = i;
                best_numeric = st.numeric_count;
            }
        }
        if (best < max_columns) return best;
        return std::nullopt;
    };

    auto inRange = [](const ColumnStats& st, double min, double max) {
        if (st.numeric_count == 0) return false;
        return st.min_value >= min - 1e-6 && st.max_value <= max + 1e-6;
    };

    auto pickInclinationByStats = [&](std::optional<size_t> depth_idx) -> std::optional<size_t> {
        size_t best = max_columns;
        for (size_t i = 0; i < stats.size(); ++i) {
            if (depth_idx.has_value() && i == depth_idx.value()) continue;
            if (inRange(stats[i], 0.0, 180.0)) {
                best = i;
                break;
            }
        }
        if (best < max_columns) return best;
        return std::nullopt;
    };

    auto pickAzimuthByStats = [&](std::optional<size_t> depth_idx, std::optional<size_t> inc_idx) -> std::optional<size_t> {
        size_t best = max_columns;
        for (size_t i = 0; i < stats.size(); ++i) {
            if (depth_idx.has_value() && i == depth_idx.value()) continue;
            if (inc_idx.has_value() && i == inc_idx.value()) continue;
            if (inRange(stats[i], 0.0, 360.0)) {
                best = i;
                break;
            }
        }
        if (best < max_columns) return best;
        return std::nullopt;
    };

    // Fallback по статистике, если заголовки не помогли
    if (!result.suggested_mapping.depth_column.has_value()) {
        auto depth_by_stats = pickDepthByStats();
        if (depth_by_stats.has_value()) {
            result.suggested_mapping.depth_column = depth_by_stats;
            result.diagnostics.push_back("Глубина выбрана по статистике (монотонный столбец)");
        }
    }

    if (!result.suggested_mapping.inclination_column.has_value()) {
        auto inc_by_stats = pickInclinationByStats(result.suggested_mapping.depth_column);
        if (inc_by_stats.has_value()) {
            result.suggested_mapping.inclination_column = inc_by_stats;
            result.diagnostics.push_back("Зенитный угол выбран по диапазону значений [0;180]");
        }
    }

    if (!result.suggested_mapping.magnetic_azimuth_column.has_value()) {
        auto az_by_stats = pickAzimuthByStats(
            result.suggested_mapping.depth_column,
            result.suggested_mapping.inclination_column
        );
        if (az_by_stats.has_value()) {
            result.suggested_mapping.magnetic_azimuth_column = az_by_stats;
            result.diagnostics.push_back("Азимут выбран по диапазону [0;360]");
        }
    }

    // Оценка уверенности
    result.confidence = 0.3;
    if (result.suggested_mapping.depth_column.has_value()) result.confidence += 0.25;
    if (result.suggested_mapping.inclination_column.has_value()) result.confidence += 0.25;
    if (result.suggested_mapping.magnetic_azimuth_column.has_value()) result.confidence += 0.1;
    if (result.has_header) result.confidence += 0.1;
    result.confidence = std::min(1.0, result.confidence);

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

    // Автоопределяем формат
    auto detection = detectCsvFormat(path);

    // Определяем кодировку и разделители
    std::string encoding = options.encoding;
    if (encoding.empty() || encoding == "AUTO") {
        encoding = detection.detected_encoding;
    }

    const char delimiter = options.delimiter.value_or(detection.detected_delimiter);
    const char decimal_separator = options.decimal_separator.value_or(detection.detected_decimal);
    const bool has_header = options.has_header.has_value()
        ? options.has_header.value()
        : detection.has_header;

    std::ifstream file(path);
    if (!file) {
        throw CsvReadError("Не удалось открыть файл: " + path.string());
    }

    // Получаем маппинг (или автоопределяем)
    CsvFieldMapping mapping = options.mapping;
    if (!mapping.isValid()) {
        mapping = detection.suggested_mapping;
    }

    if (!mapping.isValid()) {
        std::ostringstream oss;
        oss << "Не удалось определить колонки глубины и зенитного угла.";
        if (!detection.header_names.empty()) {
            oss << " Найдены заголовки: ";
            for (size_t i = 0; i < detection.header_names.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << '"' << trim(detection.header_names[i]) << '"';
            }
            oss << ". Переименуйте колонки (например, MD/DEPTH/ГЛУБИНА и INC/INCL/УГОЛ/ЗЕНИТ) или задайте маппинг вручную.";
        } else {
            oss << " В файле не обнаружен заголовок, попробуйте указать маппинг явно или добавить строку с названиями колонок.";
        }
        if (!detection.diagnostics.empty()) {
            oss << " Детали: ";
            for (size_t i = 0; i < detection.diagnostics.size(); ++i) {
                if (i > 0) oss << " ";
                oss << detection.diagnostics[i];
            }
        }
        throw CsvReadError(oss.str());
    }

    std::string line;
    size_t line_num = 0;

    // Пропускаем начальные строки
    while (line_num < options.skip_lines && std::getline(file, line)) {
        ++line_num;
    }

    // Пропускаем заголовок
    if (has_header && std::getline(file, line)) {
        ++line_num;
    }

    // Чтение данных
    while (std::getline(file, line)) {
        ++line_num;

        if (encoding == "CP1251") {
            line = convertCp1251ToUtf8(line);
        }

        line = trim(stripBom(line));
        if (line.empty()) continue;

        auto fields = splitLine(line, delimiter);

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
        double depth = parseDouble(fields[*mapping.depth_column], decimal_separator);
        auto isTextField = [](const std::string& value) {
            return std::none_of(value.begin(), value.end(), [](unsigned char ch) {
                return std::isdigit(ch);
            });
        };

        if (std::isnan(depth)) {
            if (isTextField(fields[*mapping.depth_column]) &&
                isTextField(fields[*mapping.inclination_column])) {
                continue;  // Пропускаем строки единиц измерения/комментариев
            }
        }
        if (std::isnan(depth)) {
            throw CsvReadError(
                "Некорректное значение глубины: " + fields[*mapping.depth_column] +
                ". Проверьте разделитель десятичной части и колонку глубины.",
                line_num
            );
        }
        point.depth = Meters{depth};

        // Зенитный угол
        double inc = parseDouble(fields[*mapping.inclination_column], decimal_separator);
        if (std::isnan(inc)) {
            throw CsvReadError(
                "Некорректное значение зенитного угла: " + fields[*mapping.inclination_column] +
                ". Ожидается число в диапазоне [0;180].",
                line_num
            );
        }
        point.inclination = Degrees{inc};

        // Магнитный азимут (опционально)
        if (mapping.magnetic_azimuth_column.has_value() &&
            *mapping.magnetic_azimuth_column < fields.size()) {
            double az = parseDouble(fields[*mapping.magnetic_azimuth_column], decimal_separator);
            if (!std::isnan(az)) {
                point.magnetic_azimuth = Degrees{az};
            }
        }

        // Истинный азимут (опционально)
        if (mapping.true_azimuth_column.has_value() &&
            *mapping.true_azimuth_column < fields.size()) {
            double az = parseDouble(fields[*mapping.true_azimuth_column], decimal_separator);
            if (!std::isnan(az)) {
                point.true_azimuth = Degrees{az};
            }
        }

        // Положение отклонителя (опционально)
        if (mapping.rotation_column.has_value() &&
            *mapping.rotation_column < fields.size()) {
            double rot = parseDouble(fields[*mapping.rotation_column], decimal_separator);
            if (!std::isnan(rot)) {
                point.rotation = rot;
            }
        }

        // Скорость проходки (опционально)
        if (mapping.rop_column.has_value() &&
            *mapping.rop_column < fields.size()) {
            double rop = parseDouble(fields[*mapping.rop_column], decimal_separator);
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

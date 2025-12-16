/**
 * @file zak_reader.cpp
 * @brief Реализация импорта данных из формата ZAK
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "zak_reader.hpp"
#include "csv_reader.hpp"  // Для convertCp1251ToUtf8
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>

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

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::string current;
    for (char c : str) {
        if (c == delimiter) {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    result.push_back(trim(current));
    return result;
}

double parseDouble(const std::string& str) {
    if (str.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    try {
        return std::stod(str);
    } catch (...) {
        // Попробуем заменить запятую на точку
        std::string normalized = str;
        std::replace(normalized.begin(), normalized.end(), ',', '.');
        try {
            return std::stod(normalized);
        } catch (...) {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }
}

enum class ZakSection {
    None,
    Header,
    Measurements,
    Results,
    ProjectPoints
};

} // anonymous namespace

bool canReadZak(const std::filesystem::path& path) noexcept {
    try {
        if (!std::filesystem::exists(path)) {
            return false;
        }

        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (ext != ".zak") {
            return false;
        }

        // Проверяем наличие секции #HEADER
        std::ifstream file(path);
        if (!file) {
            return false;
        }

        std::string line;
        int lines_checked = 0;
        while (std::getline(file, line) && lines_checked < 20) {
            ++lines_checked;
            line = trim(line);
            if (line.empty()) continue;

            std::string upper = toUpper(line);
            if (upper == "#HEADER" || upper.find("#HEADER") == 0) {
                return true;
            }
        }

        return false;
    } catch (...) {
        return false;
    }
}

IntervalData readZak(const std::filesystem::path& path) {
    IntervalData data;

    std::ifstream file(path);
    if (!file) {
        throw ZakReadError("Не удалось открыть файл: " + path.string());
    }

    ZakSection section = ZakSection::None;
    std::string line;
    size_t line_num = 0;

    // Для секции MEASUREMENTS
    std::vector<std::string> column_headers;
    int depth_col = -1;
    int inc_col = -1;
    int az_col = -1;
    int true_az_col = -1;

    // Автоопределение кодировки по первым строкам
    bool is_cp1251 = false;
    {
        std::string preview;
        file.seekg(0);
        std::getline(file, preview);
        for (char c : preview) {
            auto uc = static_cast<unsigned char>(c);
            // Диапазон русских букв в CP1251: 0xC0-0xFF
            // unsigned char всегда <= 0xFF, проверяем только нижнюю границу
            if (uc >= 0xC0) {
                is_cp1251 = true;
                break;
            }
        }
        file.seekg(0);
    }

    while (std::getline(file, line)) {
        ++line_num;

        // Конвертируем кодировку при необходимости
        if (is_cp1251) {
            line = convertCp1251ToUtf8(line);
        }

        line = trim(line);
        if (line.empty()) continue;

        // Проверяем секции
        std::string upper_line = toUpper(line);
        if (upper_line == "#HEADER" || upper_line.find("#HEADER") == 0) {
            section = ZakSection::Header;
            continue;
        }
        if (upper_line == "#MEASUREMENTS" || upper_line.find("#MEASUREMENTS") == 0) {
            section = ZakSection::Measurements;
            column_headers.clear();
            continue;
        }
        if (upper_line == "#RESULTS" || upper_line.find("#RESULTS") == 0) {
            section = ZakSection::Results;
            continue;
        }
        if (upper_line == "#PROJECT_POINTS" || upper_line.find("#PROJECT_POINTS") == 0) {
            section = ZakSection::ProjectPoints;
            continue;
        }
        if (upper_line == "#END" || upper_line.find("#END") == 0) {
            break;
        }

        switch (section) {
            case ZakSection::Header: {
                // Формат: KEY=VALUE
                size_t eq_pos = line.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = toUpper(trim(line.substr(0, eq_pos)));
                    std::string value = trim(line.substr(eq_pos + 1));

                    if (key == "VERSION") {
                        // Проверяем версию
                    } else if (key == "WELL") {
                        data.well = value;
                    } else if (key == "CLUSTER") {
                        data.cluster = value;
                    } else if (key == "FIELD") {
                        data.field = value;
                    } else if (key == "DATE") {
                        data.study_date = value;
                    } else if (key == "ALTITUDE" || key == "ALT") {
                        double alt = parseDouble(value);
                        if (!std::isnan(alt)) {
                            data.rotor_table_altitude = Meters{alt};
                        }
                    } else if (key == "GROUND_ALTITUDE" || key == "ALTLAND") {
                        double alt = parseDouble(value);
                        if (!std::isnan(alt)) {
                            data.ground_altitude = Meters{alt};
                        }
                    } else if (key == "DECLINATION" || key == "MSCLON") {
                        double decl = parseDouble(value);
                        if (!std::isnan(decl)) {
                            data.magnetic_declination = Degrees{decl};
                        }
                    } else if (key == "INTERVAL_START" || key == "START") {
                        double val = parseDouble(value);
                        if (!std::isnan(val)) {
                            data.interval_start = Meters{val};
                        }
                    } else if (key == "INTERVAL_END" || key == "STOP" || key == "END") {
                        double val = parseDouble(value);
                        if (!std::isnan(val)) {
                            data.interval_end = Meters{val};
                        }
                    } else if (key == "REGION") {
                        data.region = value;
                    } else if (key == "CONTRACTOR") {
                        data.contractor = value;
                    }
                }
                break;
            }

            case ZakSection::Measurements: {
                // Определяем разделитель
                char delimiter = ';';
                if (line.find('\t') != std::string::npos) {
                    delimiter = '\t';
                } else if (line.find(',') != std::string::npos && line.find(';') == std::string::npos) {
                    delimiter = ',';
                }

                auto fields = split(line, delimiter);

                // Первая строка данных - либо заголовок, либо числа
                if (column_headers.empty()) {
                    // Проверяем, это заголовок или данные
                    bool is_header = false;
                    for (const auto& f : fields) {
                        try {
                            std::stod(f);
                        } catch (...) {
                            if (!f.empty()) {
                                is_header = true;
                                break;
                            }
                        }
                    }

                    if (is_header) {
                        column_headers = fields;
                        // Определяем индексы колонок
                        for (size_t i = 0; i < fields.size(); ++i) {
                            std::string h = toUpper(fields[i]);
                            if (h == "MD" || h == "DEPTH" || h == "ГЛУБИНА" || h == "ГЛ") {
                                depth_col = static_cast<int>(i);
                            } else if (h == "INC" || h == "INCL" || h == "УГОЛ" || h == "ЗЕНИТ") {
                                inc_col = static_cast<int>(i);
                            } else if (h == "AZ" || h == "AZIM" || h == "АЗИМУТ") {
                                az_col = static_cast<int>(i);
                            } else if (h == "AZIT" || h == "AZ_TRUE" || h == "АЗИМУТ_ИСТ") {
                                true_az_col = static_cast<int>(i);
                            }
                        }

                        // Если колонки не определены, пробуем стандартный порядок
                        if (depth_col < 0 && fields.size() >= 1) depth_col = 0;
                        if (inc_col < 0 && fields.size() >= 2) inc_col = 1;
                        if (az_col < 0 && fields.size() >= 3) az_col = 2;

                        continue;
                    } else {
                        // Нет заголовка - используем порядок по умолчанию
                        depth_col = 0;
                        inc_col = 1;
                        az_col = 2;
                        if (fields.size() >= 4) true_az_col = 3;
                    }
                }

                // Парсим строку данных
                if (depth_col < 0 || inc_col < 0) {
                    throw ZakReadError("Не удалось определить колонки глубины и угла", line_num);
                }

                if (static_cast<size_t>(depth_col) >= fields.size() ||
                    static_cast<size_t>(inc_col) >= fields.size()) {
                    continue;  // Пропускаем некорректные строки
                }

                double depth = parseDouble(fields[static_cast<size_t>(depth_col)]);
                double inc = parseDouble(fields[static_cast<size_t>(inc_col)]);

                if (std::isnan(depth) || std::isnan(inc)) {
                    continue;  // Пропускаем строки с некорректными значениями
                }

                MeasurementPoint point;
                point.depth = Meters{depth};
                point.inclination = Degrees{inc};

                // Магнитный азимут
                if (az_col >= 0 && static_cast<size_t>(az_col) < fields.size()) {
                    double az = parseDouble(fields[static_cast<size_t>(az_col)]);
                    if (!std::isnan(az)) {
                        point.magnetic_azimuth = Degrees{az};
                    }
                }

                // Истинный азимут
                if (true_az_col >= 0 && static_cast<size_t>(true_az_col) < fields.size()) {
                    double az = parseDouble(fields[static_cast<size_t>(true_az_col)]);
                    if (!std::isnan(az)) {
                        point.true_azimuth = Degrees{az};
                    }
                }

                data.measurements.push_back(point);
                break;
            }

            default:
                // Игнорируем другие секции
                break;
        }
    }

    if (data.measurements.empty()) {
        throw ZakReadError("Файл не содержит данных замеров");
    }

    // Заполняем имя скважины из имени файла если пусто
    if (data.well.empty()) {
        data.well = path.stem().string();
    }

    return data;
}

} // namespace incline::io

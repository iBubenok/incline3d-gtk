/**
 * @file zak_writer.cpp
 * @brief Реализация экспорта замеров в формат ZAK
 */

#include "zak_writer.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace incline::io {

namespace {

bool isCp1251Encoding(const std::string& encoding) {
    std::string upper = encoding;
    std::transform(upper.begin(), upper.end(), upper.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper == "CP1251" || upper == "WINDOWS-1251";
}

std::string convertUtf8ToCp1251(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        auto c = static_cast<unsigned char>(input[i]);

        if (c < 0x80) {
            result += static_cast<char>(c);
            continue;
        }

        // UTF-8 кириллица: D0 80 - D1 BF
        if (c == 0xD0 && i + 1 < input.size()) {
            auto c2 = static_cast<unsigned char>(input[i + 1]);
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
            }
        } else if (c == 0xD1 && i + 1 < input.size()) {
            auto c2 = static_cast<unsigned char>(input[i + 1]);
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
        return {};
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    std::string result = ss.str();

    if (decimal_sep != '.') {
        for (char& c : result) {
            if (c == '.') {
                c = decimal_sep;
            }
        }
    }

    return result;
}

std::string formatOptionalAngle(const OptionalAngle& angle, int precision, char decimal_sep) {
    if (!angle.has_value()) {
        return {};
    }
    return formatDouble(angle->value, precision, decimal_sep);
}

void writeHeaderField(std::ostream& out, std::string_view key, const std::string& value, const std::string& eol) {
    if (value.empty()) {
        return;
    }
    out << key << "=" << value << eol;
}

void writeHeaderField(std::ostream& out, std::string_view key, double value, int precision, char decimal_sep, const std::string& eol) {
    std::string formatted = formatDouble(value, precision, decimal_sep);
    if (!formatted.empty()) {
        out << key << "=" << formatted << eol;
    }
}

} // namespace

void writeZak(
    const IntervalData& data,
    const std::filesystem::path& path,
    const ZakWriteOptions& options
) {
    if (data.measurements.empty()) {
        throw ZakWriteError("Нет данных замеров для записи");
    }

    // Определяем, нужна ли колонка истинного азимута
    bool has_true_azimuth = options.include_true_azimuth &&
        std::any_of(data.measurements.begin(), data.measurements.end(),
            [](const auto& m) { return m.true_azimuth.has_value(); });

    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    std::ofstream file(temp_path, std::ios::binary);
    if (!file) {
        throw ZakWriteError("Не удалось создать файл: " + path.string());
    }

    const std::string eol = options.use_crlf ? "\r\n" : "\n";

    auto writeLine = [&](const std::string& line) {
        if (isCp1251Encoding(options.encoding)) {
            std::string encoded = convertUtf8ToCp1251(line);
            file << encoded << eol;
        } else {
            file << line << eol;
        }
    };

    // === HEADER ===
    writeLine("#HEADER");
    writeLine("VERSION=1.0");

    std::ostringstream header_stream;
    writeHeaderField(header_stream, "WELL", data.well, eol);
    writeHeaderField(header_stream, "CLUSTER", data.cluster, eol);
    writeHeaderField(header_stream, "FIELD", data.field, eol);
    writeHeaderField(header_stream, "REGION", data.region, eol);
    writeHeaderField(header_stream, "DATE", data.study_date, eol);
    writeHeaderField(header_stream, "ALTITUDE", data.rotor_table_altitude.value, options.decimal_places, options.decimal_separator, eol);
    writeHeaderField(header_stream, "GROUND_ALTITUDE", data.ground_altitude.value, options.decimal_places, options.decimal_separator, eol);
    writeHeaderField(header_stream, "DECLINATION", data.magnetic_declination.value, options.decimal_places, options.decimal_separator, eol);
    writeHeaderField(header_stream, "INTERVAL_START", data.interval_start.value, options.decimal_places, options.decimal_separator, eol);
    writeHeaderField(header_stream, "INTERVAL_END", data.interval_end.value, options.decimal_places, options.decimal_separator, eol);
    writeHeaderField(header_stream, "CONTRACTOR", data.contractor, eol);
    const std::string header_block = header_stream.str();
    if (!header_block.empty()) {
        if (isCp1251Encoding(options.encoding)) {
            file << convertUtf8ToCp1251(header_block);
        } else {
            file << header_block;
        }
    }

    // === MEASUREMENTS ===
    writeLine("#MEASUREMENTS");
    std::ostringstream header;
    header << "MD" << options.delimiter
           << "INC" << options.delimiter
           << "AZ";
    if (has_true_azimuth) {
        header << options.delimiter << "AZ_TRUE";
    }
    writeLine(header.str());

    for (const auto& m : data.measurements) {
        std::ostringstream row;
        row << formatDouble(m.depth.value, options.decimal_places, options.decimal_separator) << options.delimiter
            << formatDouble(m.inclination.value, options.decimal_places, options.decimal_separator) << options.delimiter
            << formatOptionalAngle(m.magnetic_azimuth, options.decimal_places, options.decimal_separator);

        if (has_true_azimuth) {
            row << options.delimiter
                << formatOptionalAngle(m.true_azimuth, options.decimal_places, options.decimal_separator);
        }

        writeLine(row.str());
    }

    writeLine("#END");

    file.close();

    // Атомарное переименование
    try {
        std::filesystem::rename(temp_path, path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::filesystem::remove(temp_path);
        throw ZakWriteError("Ошибка сохранения файла: " + std::string(e.what()));
    }
}

} // namespace incline::io

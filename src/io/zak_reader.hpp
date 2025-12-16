/**
 * @file zak_reader.hpp
 * @brief Импорт данных из формата ZAK (формат заключений)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/interval_data.hpp"
#include <filesystem>
#include <stdexcept>

namespace incline::io {

using namespace incline::model;

/**
 * @brief Ошибка чтения ZAK файла
 */
class ZakReadError : public std::runtime_error {
public:
    explicit ZakReadError(const std::string& message)
        : std::runtime_error(message), line_number_(0) {}

    ZakReadError(const std::string& message, size_t line)
        : std::runtime_error(message), line_number_(line) {}

    size_t lineNumber() const noexcept { return line_number_; }

private:
    size_t line_number_;
};

/**
 * @brief Проверяет возможность чтения файла как ZAK
 */
[[nodiscard]] bool canReadZak(const std::filesystem::path& path) noexcept;

/**
 * @brief Читает данные замеров из ZAK файла
 *
 * Формат ZAK:
 * #HEADER
 * VERSION=1.0
 * WELL=123-1
 * ...
 * #MEASUREMENTS
 * MD;INC;AZ
 * 0.0;0.0;
 * 100.0;2.5;45.0
 * ...
 * #END
 *
 * @param path Путь к ZAK файлу
 * @return Данные интервала с замерами
 * @throws ZakReadError При ошибке чтения
 */
[[nodiscard]] IntervalData readZak(const std::filesystem::path& path);

} // namespace incline::io

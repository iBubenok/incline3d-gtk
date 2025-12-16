/**
 * @file csv_reader.hpp
 * @brief Импорт данных из CSV файлов
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/interval_data.hpp"
#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>

namespace incline::io {

using namespace incline::model;

/**
 * @brief Маппинг полей CSV на поля данных
 */
struct CsvFieldMapping {
    std::optional<size_t> depth_column;           ///< Глубина (обязательно)
    std::optional<size_t> inclination_column;     ///< Зенитный угол (обязательно)
    std::optional<size_t> magnetic_azimuth_column;///< Магнитный азимут
    std::optional<size_t> true_azimuth_column;    ///< Истинный азимут
    std::optional<size_t> rotation_column;        ///< Положение отклонителя
    std::optional<size_t> rop_column;             ///< Скорость проходки
    std::optional<size_t> marker_column;          ///< Маркер/метка

    /**
     * @brief Проверка валидности маппинга
     */
    [[nodiscard]] bool isValid() const noexcept {
        return depth_column.has_value() && inclination_column.has_value();
    }
};

/**
 * @brief Опции чтения CSV
 */
struct CsvReadOptions {
    char delimiter = ';';              ///< Разделитель полей
    char decimal_separator = '.';      ///< Десятичный разделитель
    bool has_header = true;            ///< Первая строка — заголовок
    std::string encoding = "UTF-8";    ///< Кодировка файла
    size_t skip_lines = 0;             ///< Пропустить строк в начале

    CsvFieldMapping mapping;           ///< Маппинг полей
};

/**
 * @brief Результат автоопределения формата CSV
 */
struct CsvDetectionResult {
    char detected_delimiter = ';';
    char detected_decimal = '.';
    bool has_header = true;
    CsvFieldMapping suggested_mapping;
    std::vector<std::string> header_names;  ///< Названия колонок (если есть)
    size_t column_count = 0;
    double confidence = 0.0;               ///< Уверенность 0.0-1.0
};

/**
 * @brief Ошибка чтения CSV
 */
class CsvReadError : public std::runtime_error {
public:
    CsvReadError(const std::string& message, size_t line = 0)
        : std::runtime_error(message)
        , line_(line) {}

    [[nodiscard]] size_t line() const noexcept { return line_; }

private:
    size_t line_;
};

/**
 * @brief Чтение CSV файла с данными замеров
 *
 * @param path Путь к файлу
 * @param options Опции чтения
 * @return Данные интервала (только замеры)
 * @throws CsvReadError При ошибке чтения
 */
[[nodiscard]] IntervalData readCsvMeasurements(
    const std::filesystem::path& path,
    const CsvReadOptions& options = {}
);

/**
 * @brief Автоопределение формата CSV файла
 *
 * Анализирует файл и определяет:
 * - Разделитель
 * - Наличие заголовка
 * - Маппинг полей
 *
 * @param path Путь к файлу
 * @return Результат детекции
 */
[[nodiscard]] CsvDetectionResult detectCsvFormat(
    const std::filesystem::path& path
);

/**
 * @brief Проверка, может ли файл быть прочитан как CSV
 */
[[nodiscard]] bool canReadCsv(const std::filesystem::path& path) noexcept;

/**
 * @brief Конвертация кодировки Windows-1251 в UTF-8
 */
[[nodiscard]] std::string convertCp1251ToUtf8(const std::string& input);

/**
 * @brief Определение кодировки файла
 * @return "UTF-8", "CP1251" или "UNKNOWN"
 */
[[nodiscard]] std::string detectEncoding(const std::filesystem::path& path);

} // namespace incline::io

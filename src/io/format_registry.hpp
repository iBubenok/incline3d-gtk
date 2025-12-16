/**
 * @file format_registry.hpp
 * @brief Реестр форматов файлов
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "csv_reader.hpp"
#include "csv_writer.hpp"
#include "las_reader.hpp"
#include "las_writer.hpp"
#include "zak_reader.hpp"
#include "project_io.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <variant>

namespace incline::io {

/**
 * @brief Результат определения формата
 */
enum class FormatDetectionResult {
    Detected,       ///< Формат определён
    Ambiguous,      ///< Несколько возможных форматов
    Unknown         ///< Формат не распознан
};

/**
 * @brief Поддерживаемый формат файла
 */
enum class FileFormat {
    Unknown,
    Project,    ///< .inclproj
    CSV,        ///< .csv, .txt
    LAS,        ///< .las
    ZAK         ///< .zak (формат заключений)
};

/**
 * @brief Информация о детекции формата
 */
struct DetectionInfo {
    FormatDetectionResult result = FormatDetectionResult::Unknown;
    FileFormat format = FileFormat::Unknown;
    std::vector<FileFormat> alternatives;  ///< При Ambiguous
    double confidence = 0.0;               ///< 0.0 - 1.0
    std::string error_message;             ///< При ошибке
};

/**
 * @brief Результат импорта данных
 */
struct ImportResult {
    bool success = false;
    IntervalData data;
    std::string error_message;
    FileFormat detected_format = FileFormat::Unknown;
};

/**
 * @brief Получить название формата
 */
[[nodiscard]] std::string_view getFormatName(FileFormat format) noexcept;

/**
 * @brief Получить расширения файлов для формата
 */
[[nodiscard]] std::vector<std::string> getFormatExtensions(FileFormat format) noexcept;

/**
 * @brief Определить формат файла
 *
 * @param path Путь к файлу
 * @return Информация о формате
 */
[[nodiscard]] DetectionInfo detectFormat(const std::filesystem::path& path);

/**
 * @brief Автоматический импорт данных замеров
 *
 * Определяет формат и импортирует данные.
 *
 * @param path Путь к файлу
 * @return Результат импорта
 */
[[nodiscard]] ImportResult importMeasurements(const std::filesystem::path& path);

/**
 * @brief Импорт данных из файла указанного формата
 *
 * @param path Путь к файлу
 * @param format Формат файла
 * @return Результат импорта
 */
[[nodiscard]] ImportResult importMeasurements(
    const std::filesystem::path& path,
    FileFormat format
);

/**
 * @brief Получить фильтр для диалога открытия файла (GTK)
 *
 * @return Строка фильтра в формате "Все поддерживаемые|*.csv;*.las;*.inclproj"
 */
[[nodiscard]] std::string getImportFileFilter();

/**
 * @brief Получить фильтр для диалога сохранения файла
 */
[[nodiscard]] std::string getExportFileFilter();

/**
 * @brief Получить фильтр для файлов проекта
 */
[[nodiscard]] std::string getProjectFileFilter();

} // namespace incline::io

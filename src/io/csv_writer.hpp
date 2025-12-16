/**
 * @file csv_writer.hpp
 * @brief Экспорт данных в CSV файлы
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/well_result.hpp"
#include "model/interval_data.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace incline::io {

using namespace incline::model;

/**
 * @brief Поле для экспорта
 */
enum class ExportField {
    Depth,              ///< Глубина MD
    Inclination,        ///< Зенитный угол
    MagneticAzimuth,    ///< Магнитный азимут
    TrueAzimuth,        ///< Истинный азимут
    X,                  ///< Координата X (север)
    Y,                  ///< Координата Y (восток)
    TVD,                ///< Вертикальная глубина
    ABSG,               ///< Абсолютная отметка
    Shift,              ///< Горизонтальное смещение
    DirectionAngle,     ///< Дирекционный угол
    Elongation,         ///< Удлинение
    Intensity10m,       ///< Интенсивность на 10м
    IntensityL,         ///< Интенсивность на L м
    Rotation,           ///< Положение отклонителя
    ROP,                ///< Скорость проходки
    ErrorX,             ///< Погрешность X
    ErrorY,             ///< Погрешность Y
    ErrorABSG,          ///< Погрешность ABSG
    Marker              ///< Маркер
};

/**
 * @brief Опции экспорта в CSV
 */
struct CsvExportOptions {
    char delimiter = ';';              ///< Разделитель полей
    char decimal_separator = '.';      ///< Десятичный разделитель
    bool include_header = true;        ///< Включить заголовок
    std::string encoding = "UTF-8";    ///< Кодировка (UTF-8 или CP1251)
    int decimal_places = 2;            ///< Знаков после запятой
    bool use_russian_headers = true;   ///< Русские названия колонок

    std::vector<ExportField> fields;   ///< Поля для экспорта
};

/**
 * @brief Ошибка записи CSV
 */
class CsvWriteError : public std::runtime_error {
public:
    explicit CsvWriteError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Экспорт результатов обработки в CSV
 *
 * @param result Результаты обработки скважины
 * @param path Путь к файлу
 * @param options Опции экспорта
 * @throws CsvWriteError При ошибке записи
 */
void writeCsvResults(
    const WellResult& result,
    const std::filesystem::path& path,
    const CsvExportOptions& options = {}
);

/**
 * @brief Экспорт исходных замеров в CSV
 *
 * @param data Исходные данные интервала
 * @param path Путь к файлу
 * @param options Опции экспорта
 * @throws CsvWriteError При ошибке записи
 */
void writeCsvMeasurements(
    const IntervalData& data,
    const std::filesystem::path& path,
    const CsvExportOptions& options = {}
);

/**
 * @brief Получить русское название поля
 */
[[nodiscard]] std::string_view getFieldNameRu(ExportField field) noexcept;

/**
 * @brief Получить английское название поля
 */
[[nodiscard]] std::string_view getFieldNameEn(ExportField field) noexcept;

/**
 * @brief Стандартный набор полей для экспорта результатов
 */
[[nodiscard]] std::vector<ExportField> getDefaultExportFields() noexcept;

/**
 * @brief Минимальный набор полей для экспорта
 */
[[nodiscard]] std::vector<ExportField> getMinimalExportFields() noexcept;

} // namespace incline::io

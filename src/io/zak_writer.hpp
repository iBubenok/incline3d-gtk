/**
 * @file zak_writer.hpp
 * @brief Экспорт замеров в формат ZAK (формат заключений)
 */

#pragma once

#include "model/interval_data.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>

namespace incline::io {

using namespace incline::model;

/**
 * @brief Опции экспорта ZAK
 */
struct ZakWriteOptions {
    char delimiter = ';';               ///< Разделитель полей в секции MEASUREMENTS
    char decimal_separator = '.';       ///< Десятичный разделитель
    int decimal_places = 2;             ///< Число знаков после запятой
    std::string encoding = "UTF-8";     ///< Кодировка файла (UTF-8 или CP1251)
    bool include_true_azimuth = true;   ///< Добавлять колонку истинного азимута при наличии данных
    bool use_crlf = false;              ///< Использовать CRLF для совместимости с Windows
};

/**
 * @brief Ошибка записи ZAK файла
 */
class ZakWriteError : public std::runtime_error {
public:
    explicit ZakWriteError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Запись замеров в ZAK файл
 *
 * @param data Исходные данные интервала
 * @param path Путь к файлу
 * @param options Опции экспорта
 * @throws ZakWriteError При ошибке записи
 */
void writeZak(
    const IntervalData& data,
    const std::filesystem::path& path,
    const ZakWriteOptions& options = {}
);

} // namespace incline::io

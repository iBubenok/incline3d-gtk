/**
 * @file las_reader.hpp
 * @brief Импорт данных из LAS 2.0 файлов
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/interval_data.hpp"
#include <filesystem>
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>
#include <unordered_map>

namespace incline::io {

using namespace incline::model;

/// Стандартное NULL-значение LAS
constexpr double LAS_NULL_VALUE = -999.25;

/**
 * @brief Мнемоники кривых LAS
 */
struct LasCurveMnemonics {
    std::string depth = "DEPT";        ///< Глубина
    std::string inclination = "INCL";  ///< Зенитный угол
    std::string azimuth = "AZIM";      ///< Магнитный азимут
    std::string true_azimuth = "AZIT"; ///< Истинный азимут
};

/**
 * @brief Опции чтения LAS
 */
struct LasReadOptions {
    LasCurveMnemonics mnemonics;       ///< Мнемоники кривых
    double null_value = LAS_NULL_VALUE;///< NULL-значение
    bool auto_detect_curves = true;    ///< Автоопределение кривых
};

/**
 * @brief Информация о кривой LAS
 */
struct LasCurveInfo {
    std::string mnemonic;              ///< Мнемоника
    std::string unit;                  ///< Единицы измерения
    std::string description;           ///< Описание
    size_t column_index = 0;           ///< Индекс колонки в данных
};

/**
 * @brief Результат чтения LAS файла
 */
struct LasReadResult {
    IntervalData data;                 ///< Данные замеров
    std::vector<LasCurveInfo> curves;  ///< Информация о кривых
    std::unordered_map<std::string, std::string> well_info; ///< Информация о скважине
    std::string version;               ///< Версия LAS
    double null_value = LAS_NULL_VALUE;///< NULL-значение из файла
};

/**
 * @brief Ошибка чтения LAS
 */
class LasReadError : public std::runtime_error {
public:
    LasReadError(const std::string& message, size_t line = 0)
        : std::runtime_error(message)
        , line_(line) {}

    [[nodiscard]] size_t line() const noexcept { return line_; }

private:
    size_t line_;
};

/**
 * @brief Чтение LAS 2.0 файла
 *
 * @param path Путь к файлу
 * @param options Опции чтения
 * @return Результат чтения
 * @throws LasReadError При ошибке чтения
 */
[[nodiscard]] LasReadResult readLas(
    const std::filesystem::path& path,
    const LasReadOptions& options = {}
);

/**
 * @brief Чтение только данных замеров из LAS
 *
 * @param path Путь к файлу
 * @param options Опции чтения
 * @return Данные интервала
 * @throws LasReadError При ошибке чтения
 */
[[nodiscard]] IntervalData readLasMeasurements(
    const std::filesystem::path& path,
    const LasReadOptions& options = {}
);

/**
 * @brief Проверка, является ли файл LAS форматом
 */
[[nodiscard]] bool canReadLas(const std::filesystem::path& path) noexcept;

/**
 * @brief Получить список кривых в LAS файле
 */
[[nodiscard]] std::vector<LasCurveInfo> getLasCurves(
    const std::filesystem::path& path
);

/**
 * @brief Проверка, является ли значение NULL
 */
[[nodiscard]] inline bool isLasNull(double value, double null_value = LAS_NULL_VALUE) noexcept {
    return std::abs(value - null_value) < 1e-6;
}

} // namespace incline::io

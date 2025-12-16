/**
 * @file las_writer.hpp
 * @brief Экспорт данных в LAS 2.0 файлы
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/well_result.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace incline::io {

using namespace incline::model;

/**
 * @brief Кривая для экспорта в LAS
 */
enum class LasCurve {
    Depth,              ///< DEPT - глубина MD
    Inclination,        ///< INCL - зенитный угол
    MagneticAzimuth,    ///< AZIM - магнитный азимут
    TrueAzimuth,        ///< AZIT - истинный азимут
    TVD,                ///< TVD - вертикальная глубина
    TVDSS,              ///< TVDSS - TVD от уровня моря
    North,              ///< NORTH - координата X (север)
    East,               ///< EAST - координата Y (восток)
    DLS                 ///< DLS - dogleg severity (°/30м)
};

/**
 * @brief Опции экспорта в LAS
 */
struct LasExportOptions {
    double null_value = -999.25;       ///< NULL-значение
    int decimal_places = 2;            ///< Знаков после запятой
    bool wrap = false;                 ///< Перенос строк (WRAP=YES)

    std::string company;               ///< Компания
    std::string service_company;       ///< Сервисная компания
    std::string country = "Russia";    ///< Страна
    std::string date;                  ///< Дата (если пусто - текущая)

    std::vector<LasCurve> curves;      ///< Кривые для экспорта
};

/**
 * @brief Ошибка записи LAS
 */
class LasWriteError : public std::runtime_error {
public:
    explicit LasWriteError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Экспорт результатов обработки в LAS 2.0
 *
 * @param result Результаты обработки скважины
 * @param path Путь к файлу
 * @param options Опции экспорта
 * @throws LasWriteError При ошибке записи
 */
void writeLas(
    const WellResult& result,
    const std::filesystem::path& path,
    const LasExportOptions& options = {}
);

/**
 * @brief Получить мнемонику кривой
 */
[[nodiscard]] std::string_view getLasCurveMnemonic(LasCurve curve) noexcept;

/**
 * @brief Получить единицы измерения кривой
 */
[[nodiscard]] std::string_view getLasCurveUnit(LasCurve curve) noexcept;

/**
 * @brief Получить описание кривой
 */
[[nodiscard]] std::string_view getLasCurveDescription(LasCurve curve) noexcept;

/**
 * @brief Стандартный набор кривых для экспорта
 */
[[nodiscard]] std::vector<LasCurve> getDefaultLasCurves() noexcept;

} // namespace incline::io

/**
 * @file project_point.hpp
 * @brief Проектная точка (целевой пласт)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "types.hpp"
#include <string>
#include <optional>
#include <vector>

namespace incline::model {

/**
 * @brief Фактические параметры проектной точки
 *
 * Заполняется после обработки траектории путём интерполяции.
 */
struct ProjectPointFactual {
    Degrees inclination{0.0};            ///< Фактический зенитный угол
    OptionalAngle magnetic_azimuth;      ///< Фактический магнитный азимут
    OptionalAngle true_azimuth;          ///< Фактический истинный азимут
    Meters shift{0.0};                   ///< Фактическое смещение
    Meters elongation{0.0};              ///< Фактическое удлинение
    Meters x{0.0};                       ///< Фактическая координата X
    Meters y{0.0};                       ///< Фактическая координата Y
    Meters deviation{0.0};               ///< Отход от проектной точки
    Degrees deviation_direction{0.0};    ///< Дирекционный угол отхода
    Meters tvd{0.0};                     ///< Фактическая вертикальная глубина
    double intensity_10m = 0.0;          ///< Интенсивность на 10м
    double intensity_L = 0.0;            ///< Интенсивность на L м
};

/**
 * @brief Проектная точка (целевой пласт)
 *
 * Эквивалент структуры Project_Points в Delphi.
 */
struct ProjectPoint {
    // === Идентификация ===
    std::string name;                    ///< PlastName - название пласта

    // === Проектные параметры ===
    OptionalAngle azimuth_geographic;    ///< AzimGeogr - географический/дирекц. азимут
    Meters shift{0.0};                   ///< Shift - проектное смещение

    // Глубина задаётся ОДНИМ из способов:
    std::optional<Meters> depth;         ///< Depth - глубина по стволу (MD)
    std::optional<Meters> abs_depth;     ///< AbsDepth - абсолютная отметка

    Meters radius{50.0};                 ///< Radius - радиус допуска (круг на плане)

    // === Базовая точка (если проектная точка задана относительно) ===
    std::optional<Meters> base_shift;           ///< BaseShift
    std::optional<OptionalAngle> base_azimuth;  ///< BaseAzimGeogr
    std::optional<Meters> base_depth;           ///< BaseDepth

    // === Фактические параметры (рассчитываются при обработке) ===
    std::optional<ProjectPointFactual> factual;

    /**
     * @brief Проверка попадания в круг допуска
     */
    [[nodiscard]] bool withinTolerance() const noexcept {
        if (!factual.has_value()) {
            return false;
        }
        return factual->deviation.value <= radius.value;
    }

    /**
     * @brief Получить проектные координаты X, Y
     * @return Пара {X, Y} или nullopt если нет азимута
     */
    [[nodiscard]] std::optional<std::pair<Meters, Meters>> getProjectedCoordinates() const noexcept {
        if (!azimuth_geographic.has_value()) {
            return std::nullopt;
        }

        double az_rad = azimuth_geographic->value * std::numbers::pi / 180.0;

        // Если есть базовая точка
        if (base_shift.has_value() && base_azimuth.has_value() && base_azimuth->has_value()) {
            double base_az_rad = base_azimuth.value().value().value * std::numbers::pi / 180.0;
            double base_x = base_shift->value * std::cos(base_az_rad);
            double base_y = base_shift->value * std::sin(base_az_rad);
            double proj_x = base_x + shift.value * std::cos(az_rad);
            double proj_y = base_y + shift.value * std::sin(az_rad);
            return std::make_pair(Meters{proj_x}, Meters{proj_y});
        }

        // Относительно устья
        double proj_x = shift.value * std::cos(az_rad);
        double proj_y = shift.value * std::sin(az_rad);
        return std::make_pair(Meters{proj_x}, Meters{proj_y});
    }

    /**
     * @brief Проверка валидности проектной точки
     */
    [[nodiscard]] bool isValid() const noexcept {
        // Должна быть задана хотя бы одна глубина
        return depth.has_value() || abs_depth.has_value();
    }
};

/**
 * @brief Массив проектных точек
 */
using ProjectPointList = std::vector<ProjectPoint>;

} // namespace incline::model

/**
 * @file processed_point.hpp
 * @brief Результат обработки точки траектории
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "types.hpp"
#include <vector>
#include <optional>
#include <string>

namespace incline::model {

/**
 * @brief Результат обработки одной точки траектории
 *
 * Эквивалент структуры РЕЗ_ОБР_ИНКЛ в Delphi.
 * Содержит исходные данные, рассчитанные координаты, интенсивности и погрешности.
 */
struct ProcessedPoint {
    // === Исходные данные (копия из MeasurementPoint) ===
    Meters depth{0.0};                   ///< Глубина MD
    Degrees inclination{0.0};            ///< Зенитный угол
    OptionalAngle magnetic_azimuth;      ///< Магнитный азимут
    OptionalAngle true_azimuth;          ///< Истинный/дирекционный азимут
    OptionalAngle computed_azimuth;      ///< Использованный (рассчитанный/интерполированный) азимут

    // === Дополнительные параметры замера (если есть) ===
    std::optional<double> rotation;      ///< ВРАЩ - скорость вращения
    std::optional<double> rop;           ///< СКОР - скорость проходки
    std::optional<std::string> marker;   ///< МЕТКА - маркер

    // === Рассчитанные величины ===
    Meters elongation{0.0};              ///< УДЛГ - удлинение (MD - TVD)
    Meters shift{0.0};                   ///< СМГ - горизонтальное смещение от устья
    Degrees direction_angle{0.0};        ///< ДУГ - дирекционный угол смещения

    Meters x{0.0};                       ///< X - смещение на север
    Meters y{0.0};                       ///< Y - смещение на восток
    Meters tvd{0.0};                     ///< Вертикальная глубина (TVD)
    Meters absg{0.0};                    ///< Абсолютная отметка (ALTrot - TVD)

    double intensity_10m = 0.0;          ///< ИНТГ - интенсивность на 10м (°/10м)
    double intensity_L = 0.0;            ///< ИНТГ2 - интенсивность на L м (°/L м)

    // === Погрешности ===
    Meters error_x{0.0};                 ///< ОШ_СМ_X - погрешность X
    Meters error_y{0.0};                 ///< ОШ_СМ_Y - погрешность Y
    Meters error_absg{0.0};              ///< ОШ_АБСГ - погрешность абс. отметки
    double error_intensity = 0.0;        ///< ОШ_ИНТГ - погрешность интенсивности

    // === Плановые значения (если заданы) ===
    std::optional<Meters> planned_tvd;           ///< Верт_план
    std::optional<Meters> planned_x;             ///< X_план
    std::optional<Meters> planned_y;             ///< Y_план
    std::optional<double> planned_intensity_10m; ///< ИНТГ_план
    std::optional<double> planned_intensity_L;   ///< ИНТГ2_план

    /**
     * @brief Получить 3D координату точки
     */
    [[nodiscard]] Coordinate3D coordinate() const noexcept {
        return {x, y, tvd};
    }

    /**
     * @brief Получить горизонтальное смещение (вычисленное из X,Y)
     */
    [[nodiscard]] Meters calculatedShift() const noexcept {
        return Meters{std::sqrt(x.value * x.value + y.value * y.value)};
    }

    /**
     * @brief Получить дирекционный угол (вычисленный из X,Y)
     */
    [[nodiscard]] OptionalAngle calculatedDirection() const noexcept {
        if (std::abs(x.value) < 1e-9 && std::abs(y.value) < 1e-9) {
            return std::nullopt;
        }
        double angle = std::atan2(y.value, x.value) * 180.0 / std::numbers::pi;
        if (angle < 0.0) {
            angle += 360.0;
        }
        return Degrees{angle};
    }
};

/**
 * @brief Массив обработанных точек
 */
using ProcessedPointList = std::vector<ProcessedPoint>;

} // namespace incline::model

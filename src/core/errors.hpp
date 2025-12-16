/**
 * @file errors.hpp
 * @brief Расчёт погрешностей координат
 * @author Yan Bubenok <yan@bubenok.com>
 *
 * Реализация расчёта распространения ошибок измерений
 * на координаты траектории.
 */

#pragma once

#include "model/types.hpp"

namespace incline::core {

using namespace incline::model;

/**
 * @brief Вклад погрешностей от одного интервала
 */
struct ErrorContribution {
    double var_x = 0.0;   ///< Дисперсия X
    double var_y = 0.0;   ///< Дисперсия Y
    double var_z = 0.0;   ///< Дисперсия Z (TVD)
};

/**
 * @brief Накопленные погрешности
 */
struct AccumulatedErrors {
    double var_x = 0.0;   ///< Накопленная дисперсия X
    double var_y = 0.0;   ///< Накопленная дисперсия Y
    double var_z = 0.0;   ///< Накопленная дисперсия Z

    /**
     * @brief Добавить вклад от интервала
     */
    void add(const ErrorContribution& contrib) noexcept {
        var_x += contrib.var_x;
        var_y += contrib.var_y;
        var_z += contrib.var_z;
    }

    /**
     * @brief Сбросить накопления
     */
    void reset() noexcept {
        var_x = 0.0;
        var_y = 0.0;
        var_z = 0.0;
    }
};

/**
 * @brief Погрешности с 95% доверительным интервалом
 */
struct Errors95 {
    Meters error_x{0.0};   ///< Погрешность X (95%)
    Meters error_y{0.0};   ///< Погрешность Y (95%)
    Meters error_z{0.0};   ///< Погрешность Z (95%)
};

/**
 * @brief Расчёт вклада погрешностей от одного интервала
 *
 * Использует метод распространения ошибок через частные производные.
 *
 * @param depth1 Глубина начальной точки
 * @param depth2 Глубина конечной точки
 * @param inc1 Зенитный угол начальной точки
 * @param inc2 Зенитный угол конечной точки
 * @param az1 Азимут начальной точки
 * @param az2 Азимут конечной точки
 * @param sigma_inc Погрешность измерения зенитного угла (°)
 * @param sigma_az Погрешность измерения азимута (°)
 * @param sigma_depth Погрешность измерения глубины (м)
 * @return Вклад в дисперсии координат
 */
[[nodiscard]] ErrorContribution calculateIntervalErrors(
    Meters depth1, Meters depth2,
    Degrees inc1, Degrees inc2,
    OptionalAngle az1, OptionalAngle az2,
    Degrees sigma_inc,
    Degrees sigma_az,
    Meters sigma_depth = Meters{0.0}
) noexcept;

/**
 * @brief Преобразование накопленных дисперсий в погрешности с 95% ДИ
 *
 * @param acc Накопленные дисперсии
 * @return Погрешности (σ × 1.96)
 */
[[nodiscard]] Errors95 getErrors95(const AccumulatedErrors& acc) noexcept;

/**
 * @brief Расчёт погрешности интенсивности
 *
 * @param intensity_10m Рассчитанная интенсивность
 * @param sigma_inc Погрешность зенитного угла
 * @param sigma_az Погрешность азимута
 * @param interval_length Длина интервала
 * @return Погрешность интенсивности в °/10м
 */
[[nodiscard]] double calculateIntensityError(
    double intensity_10m,
    Degrees sigma_inc,
    Degrees sigma_az,
    Meters interval_length
) noexcept;

/**
 * @brief Коэффициент для 95% доверительного интервала
 */
constexpr double kConfidence95 = 1.96;

/**
 * @brief Эмпирический делитель для дисперсий (из анализа Delphi)
 *
 * Примечание: требует валидации на тестовых данных.
 */
constexpr double kVarianceDivisor = 2.0;

} // namespace incline::core

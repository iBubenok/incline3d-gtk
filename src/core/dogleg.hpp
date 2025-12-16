/**
 * @file dogleg.hpp
 * @brief Расчёт пространственной интенсивности (dogleg)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/types.hpp"

namespace incline::core {

using namespace incline::model;

/**
 * @brief Расчёт угла искривления (dogleg angle) между двумя точками
 *
 * Dogleg - угол между направлениями ствола скважины в двух точках.
 *
 * Формула (косинусная):
 * cos(DL) = cos(θ2-θ1) - sin(θ1)*sin(θ2)*(1 - cos(φ2-φ1))
 *
 * @param inc1 Зенитный угол в первой точке
 * @param az1 Азимут в первой точке
 * @param inc2 Зенитный угол во второй точке
 * @param az2 Азимут во второй точке
 * @return Угол искривления в радианах
 */
[[nodiscard]] Radians calculateDogleg(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт угла искривления через формулу синуса
 *
 * Альтернативный метод, может быть более стабилен для малых углов.
 *
 * sin²(DL/2) = sin²((θ2-θ1)/2) + sin²((θ2+θ1)/2) * sin²((φ2-φ1)/2)
 */
[[nodiscard]] Radians calculateDoglegSin(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт интенсивности искривления на 10 метров
 *
 * INT10 = DL_deg * (10 / L)
 *
 * @param depth1 Глубина первой точки
 * @param inc1 Зенитный угол первой точки
 * @param az1 Азимут первой точки
 * @param depth2 Глубина второй точки
 * @param inc2 Зенитный угол второй точки
 * @param az2 Азимут второй точки
 * @return Интенсивность в °/10м
 */
[[nodiscard]] double calculateIntensity10m(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт интенсивности искривления на заданный интервал L
 *
 * INT_L = DL_deg * (L / interval)
 *
 * @param depth1 Глубина первой точки
 * @param inc1 Зенитный угол первой точки
 * @param az1 Азимут первой точки
 * @param depth2 Глубина второй точки
 * @param inc2 Зенитный угол второй точки
 * @param az2 Азимут второй точки
 * @param interval_L Интервал для расчёта (обычно 25м)
 * @return Интенсивность в °/L м
 */
[[nodiscard]] double calculateIntensityL(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2,
    Meters interval_L
) noexcept;

/**
 * @brief Расчёт зенитной интенсивности (только по зенитному углу)
 *
 * Без учёта азимута - только изменение зенитного угла.
 *
 * @return Зенитная интенсивность в °/10м
 */
[[nodiscard]] double calculateZenithIntensity10m(
    Meters depth1, Degrees inc1,
    Meters depth2, Degrees inc2
) noexcept;

/**
 * @brief Расчёт азимутальной интенсивности
 *
 * Только изменение азимута на заданном зенитном угле.
 *
 * @return Азимутальная интенсивность в °/10м
 */
[[nodiscard]] double calculateAzimuthalIntensity10m(
    Meters depth1, OptionalAngle az1,
    Meters depth2, OptionalAngle az2,
    Degrees avg_inclination
) noexcept;

} // namespace incline::core

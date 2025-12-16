/**
 * @file angle_utils.hpp
 * @brief Утилиты для работы с углами
 * @author Yan Bubenok <yan@bubenok.com>
 *
 * Функции нормализации, усреднения и интерполяции углов с учётом
 * перехода через 0°/360°.
 */

#pragma once

#include "model/types.hpp"

namespace incline::core {

using namespace incline::model;

/**
 * @brief Нормализация угла к диапазону [0°, 360°)
 *
 * Значение 360° округляется до 0°.
 */
[[nodiscard]] Degrees normalizeAngle(Degrees angle) noexcept;

/**
 * @brief Усреднение двух азимутов по короткой дуге
 *
 * Учитывает переход через 0°/360°.
 * Например: average(350°, 10°) = 0°
 *
 * @param a1 Первый азимут
 * @param a2 Второй азимут
 * @return Средний азимут или nullopt если хотя бы один отсутствует
 */
[[nodiscard]] OptionalAngle averageAzimuth(OptionalAngle a1, OptionalAngle a2) noexcept;

/**
 * @brief Усреднение двух зенитных углов
 *
 * Простое арифметическое среднее (для зенитных углов переход не нужен).
 */
[[nodiscard]] Degrees averageInclination(Degrees inc1, Degrees inc2) noexcept;

/**
 * @brief Интерполяция азимута по глубине
 *
 * Учитывает переход через 0°/360°.
 *
 * @param target_depth Целевая глубина
 * @param az1 Азимут в первой точке
 * @param depth1 Глубина первой точки
 * @param az2 Азимут во второй точке
 * @param depth2 Глубина второй точки
 * @return Интерполированный азимут или nullopt
 */
[[nodiscard]] OptionalAngle interpolateAzimuth(
    Meters target_depth,
    OptionalAngle az1, Meters depth1,
    OptionalAngle az2, Meters depth2
) noexcept;

/**
 * @brief Интерполяция зенитного угла по глубине
 */
[[nodiscard]] Degrees interpolateInclination(
    Meters target_depth,
    Degrees inc1, Meters depth1,
    Degrees inc2, Meters depth2
) noexcept;

/**
 * @brief Линейная интерполяция числового значения
 */
[[nodiscard]] inline double interpolate(
    double target,
    double v1, double d1,
    double v2, double d2
) noexcept {
    if (std::abs(d2 - d1) < 1e-9) {
        return v1;
    }
    double ratio = (target - d1) / (d2 - d1);
    return v1 + ratio * (v2 - v1);
}

/**
 * @brief Разность азимутов с учётом перехода через 0°/360°
 *
 * Возвращает разность по короткой дуге [-180°, 180°).
 */
[[nodiscard]] Degrees azimuthDifference(Degrees az1, Degrees az2) noexcept;

/**
 * @brief Проверка близости двух углов
 */
[[nodiscard]] bool anglesClose(Degrees a1, Degrees a2, Degrees tolerance = Degrees{0.01}) noexcept;

/**
 * @brief Конвертация азимута в компоненты единичного вектора
 *
 * @param inclination Зенитный угол
 * @param azimuth Азимут (или nullopt для вертикали)
 * @return {nx, ny, nz} - компоненты направляющего вектора
 */
[[nodiscard]] std::tuple<double, double, double> directionVector(
    Degrees inclination,
    OptionalAngle azimuth
) noexcept;

} // namespace incline::core

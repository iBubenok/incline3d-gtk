/**
 * @file processing.hpp
 * @brief Полная обработка данных скважины
 * @author Yan Bubenok <yan@bubenok.com>
 *
 * Координирует расчёт траектории, интенсивности, погрешностей
 * и заполнение результатов.
 */

#pragma once

#include "model/interval_data.hpp"
#include "model/well_result.hpp"
#include <functional>

namespace incline::core {

using namespace incline::model;

/**
 * @brief Опции обработки скважины
 */
struct ProcessingOptions {
    TrajectoryMethod method = TrajectoryMethod::MinimumCurvature;
    AzimuthMode azimuth_mode = AzimuthMode::Auto;
    Meters intensity_interval_L{25.0};
    VerticalityConfig verticality;
    bool calculate_errors = true;
    bool smooth_intensity = false;
    Meters smoothing_window{5.0};      ///< Окно сглаживания интенсивности
};

/**
 * @brief Callback для индикации прогресса
 *
 * @param progress Прогресс от 0.0 до 1.0
 * @param message Описание текущей операции
 */
using ProgressCallback = std::function<void(double progress, std::string_view message)>;

/**
 * @brief Полная обработка данных скважины
 *
 * Выполняет:
 * 1. Расчёт координат траектории выбранным методом
 * 2. Расчёт интенсивности (INT10 и INT_L)
 * 3. Расчёт погрешностей
 * 4. Обработку вертикальных участков
 * 5. Заполнение статистики (максимумы)
 *
 * @param data Исходные данные интервала
 * @param options Опции обработки
 * @param on_progress Callback прогресса (опционально)
 * @return Результаты обработки
 */
[[nodiscard]] WellResult processWell(
    const IntervalData& data,
    const ProcessingOptions& options = {},
    ProgressCallback on_progress = nullptr
);

/**
 * @brief Проверка, является ли точка эффективно вертикальной
 *
 * Точка считается вертикальной если:
 * - Зенитный угол ≤ критического
 * - Отсутствует азимут
 */
[[nodiscard]] bool isEffectivelyVertical(
    const MeasurementPoint& point,
    const VerticalityConfig& config
) noexcept;

/**
 * @brief Проверка, находится ли глубина в приустьевой зоне
 */
[[nodiscard]] bool isNearSurface(
    Meters depth,
    const IntervalData& data,
    const VerticalityConfig& config
) noexcept;

/**
 * @brief Сглаживание интенсивности в скользящем окне
 */
void smoothIntensity(
    ProcessedPointList& points,
    Meters window_half_size
);

/**
 * @brief Расчёт интенсивности на интервал L для всех точек
 *
 * Стратегия: для каждой точки i находится точка j, ближайшая к (MD_i - L),
 * и рассчитывается интенсивность между ними.
 */
void calculateIntensityLForAllPoints(
    ProcessedPointList& points,
    Meters interval_L
);

/**
 * @brief Интерполяция параметров для проектных точек
 *
 * Для каждой проектной точки вычисляет фактические параметры
 * путём интерполяции по результатам обработки.
 */
void interpolateProjectPoints(
    WellResult& result
);

} // namespace incline::core

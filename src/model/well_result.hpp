/**
 * @file well_result.hpp
 * @brief Результаты обработки скважины
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "processed_point.hpp"
#include "project_point.hpp"
#include <string>

namespace incline::model {

/**
 * @brief Результаты обработки скважины
 *
 * Эквивалент структуры РЕЗ_ИНКЛ в Delphi.
 */
struct WellResult {
    // === Идентификация (копируется из IntervalData) ===
    std::string uwi;
    std::string region;
    std::string field;
    std::string area;
    std::string cluster;
    std::string well;

    // === Параметры (копируются/рассчитываются) ===
    Meters rotor_table_altitude{0.0};
    Meters ground_altitude{0.0};
    Degrees magnetic_declination{0.0};
    Meters target_bottom{0.0};
    Meters current_bottom{0.0};

    // === Фактические значения забоя (рассчитанные) ===
    Meters actual_shift{0.0};            ///< Фактическое смещение забоя
    Meters shift_deviation{0.0};         ///< Отклонение по смещению

    OptionalAngle actual_bottom_azimuth; ///< Фактический азимут забоя
    Degrees actual_direction_angle{0.0}; ///< Фактический дирекционный угол смещения
    Degrees direction_angle_deviation{0.0}; ///< Отклонение по дир. углу

    Meters actual_bottom_deviation{0.0}; ///< Фактический отход забоя
    OptionalAngle deviation_azimuth;     ///< Азимут отхода забоя
    OptionalAngle true_deviation_azimuth;///< Истинный азимут отхода

    Meters target_abs_bottom{0.0};       ///< Проектная абсолютная отметка забоя
    Meters actual_abs_bottom{0.0};       ///< Фактическая абсолютная отметка забоя

    // === Положение в кусте ===
    Meters cluster_shift{0.0};           ///< Смещение в кусте
    OptionalAngle cluster_shift_azimuth; ///< Азимут смещения в кусте

    // === Максимумы ===
    Degrees max_inclination{0.0};        ///< Макс. зенитный угол
    Meters max_inclination_depth{0.0};   ///< Глубина макс. зенитного угла

    double max_intensity_10m = 0.0;      ///< Макс. интенсивность на 10м (°/10м)
    Meters max_intensity_10m_depth{0.0}; ///< Глубина макс. интенсивности 10м

    double max_intensity_L = 0.0;        ///< Макс. интенсивность на L м
    Meters max_intensity_L_depth{0.0};   ///< Глубина макс. интенсивности L
    Meters intensity_interval_L{25.0};   ///< Интервал L для расчёта интенсивности

    // === Настройки обработки ===
    AzimuthMode azimuth_mode = AzimuthMode::Auto;
    TrajectoryMethod trajectory_method = TrajectoryMethod::MinimumCurvature;

    // === Результаты по точкам ===
    ProcessedPointList points;

    // === Проектные точки ===
    ProjectPointList project_points;

    /**
     * @brief Получить отображаемое имя скважины
     */
    [[nodiscard]] std::string displayName() const {
        if (!well.empty()) {
            if (!cluster.empty()) {
                return cluster + "/" + well;
            }
            return well;
        }
        if (!uwi.empty()) {
            return uwi;
        }
        return "Безымянная скважина";
    }

    /**
     * @brief Проверка наличия результатов
     */
    [[nodiscard]] bool empty() const noexcept {
        return points.empty();
    }

    /**
     * @brief Количество обработанных точек
     */
    [[nodiscard]] size_t size() const noexcept {
        return points.size();
    }

    /**
     * @brief Получить последнюю точку (забой)
     */
    [[nodiscard]] const ProcessedPoint* bottomPoint() const noexcept {
        if (points.empty()) return nullptr;
        return &points.back();
    }

    /**
     * @brief Найти точку по глубине (ближайшую)
     */
    [[nodiscard]] const ProcessedPoint* findByDepth(Meters depth) const noexcept {
        if (points.empty()) return nullptr;

        const ProcessedPoint* closest = &points[0];
        double min_diff = std::abs(points[0].depth.value - depth.value);

        for (const auto& pt : points) {
            double diff = std::abs(pt.depth.value - depth.value);
            if (diff < min_diff) {
                min_diff = diff;
                closest = &pt;
            }
        }
        return closest;
    }

    /**
     * @brief Получить диапазон глубин
     */
    [[nodiscard]] std::pair<Meters, Meters> depthRange() const noexcept {
        if (points.empty()) {
            return {Meters{0.0}, Meters{0.0}};
        }
        return {points.front().depth, points.back().depth};
    }

    /**
     * @brief Получить диапазон TVD
     */
    [[nodiscard]] std::pair<Meters, Meters> tvdRange() const noexcept {
        if (points.empty()) {
            return {Meters{0.0}, Meters{0.0}};
        }

        Meters min_tvd = points[0].tvd;
        Meters max_tvd = points[0].tvd;

        for (const auto& pt : points) {
            if (pt.tvd.value < min_tvd.value) min_tvd = pt.tvd;
            if (pt.tvd.value > max_tvd.value) max_tvd = pt.tvd;
        }
        return {min_tvd, max_tvd};
    }

    /**
     * @brief Получить диапазон X координат
     */
    [[nodiscard]] std::pair<Meters, Meters> xRange() const noexcept {
        if (points.empty()) {
            return {Meters{0.0}, Meters{0.0}};
        }

        Meters min_x = points[0].x;
        Meters max_x = points[0].x;

        for (const auto& pt : points) {
            if (pt.x.value < min_x.value) min_x = pt.x;
            if (pt.x.value > max_x.value) max_x = pt.x;
        }
        return {min_x, max_x};
    }

    /**
     * @brief Получить диапазон Y координат
     */
    [[nodiscard]] std::pair<Meters, Meters> yRange() const noexcept {
        if (points.empty()) {
            return {Meters{0.0}, Meters{0.0}};
        }

        Meters min_y = points[0].y;
        Meters max_y = points[0].y;

        for (const auto& pt : points) {
            if (pt.y.value < min_y.value) min_y = pt.y;
            if (pt.y.value > max_y.value) max_y = pt.y;
        }
        return {min_y, max_y};
    }

    /**
     * @brief Вычислить и обновить статистику максимумов
     */
    void updateStatistics() {
        if (points.empty()) return;

        max_inclination = Degrees{0.0};
        max_intensity_10m = 0.0;
        max_intensity_L = 0.0;

        for (const auto& pt : points) {
            if (pt.inclination.value > max_inclination.value) {
                max_inclination = pt.inclination;
                max_inclination_depth = pt.depth;
            }
            if (pt.intensity_10m > max_intensity_10m) {
                max_intensity_10m = pt.intensity_10m;
                max_intensity_10m_depth = pt.depth;
            }
            if (pt.intensity_L > max_intensity_L) {
                max_intensity_L = pt.intensity_L;
                max_intensity_L_depth = pt.depth;
            }
        }

        // Обновить данные забоя
        const auto* bottom = bottomPoint();
        if (bottom) {
            actual_shift = bottom->shift;
            actual_bottom_azimuth = bottom->calculatedDirection();
            actual_direction_angle = bottom->direction_angle;
            actual_abs_bottom = bottom->absg;
        }
    }
};

} // namespace incline::model

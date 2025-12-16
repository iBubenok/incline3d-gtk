/**
 * @file processing.cpp
 * @brief Реализация полной обработки скважины
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "processing.hpp"
#include "trajectory.hpp"
#include "dogleg.hpp"
#include "errors.hpp"
#include "angle_utils.hpp"
#include <cmath>
#include <algorithm>

namespace incline::core {

bool isEffectivelyVertical(
    const MeasurementPoint& point,
    const VerticalityConfig& config
) noexcept {
    // Нет азимута → вертикальная
    if (!point.hasAzimuth()) {
        return true;
    }

    // Малый зенитный угол → вертикальная
    return point.inclination.value <= config.critical_inclination.value;
}

bool isNearSurface(
    Meters depth,
    const IntervalData& data,
    const VerticalityConfig& config
) noexcept {
    Meters boundary = data.getNearSurfaceBoundary(config.near_surface_depth);
    return depth.value <= boundary.value;
}

void smoothIntensity(
    ProcessedPointList& points,
    Meters window_half_size
) {
    if (points.empty()) return;

    // Сохраняем исходные значения
    std::vector<double> original_intensity(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        original_intensity[i] = points[i].intensity_10m;
    }

    // Сглаживание
    for (size_t i = 0; i < points.size(); ++i) {
        double depth_i = points[i].depth.value;
        double sum = 0.0;
        int count = 0;

        for (size_t j = 0; j < points.size(); ++j) {
            double depth_j = points[j].depth.value;
            if (std::abs(depth_j - depth_i) <= window_half_size.value) {
                sum += original_intensity[j];
                ++count;
            }
        }

        points[i].intensity_10m = (count > 0) ? (sum / count) : 0.0;
    }
}

void calculateIntensityLForAllPoints(
    ProcessedPointList& points,
    Meters interval_L
) {
    if (points.size() < 2) return;

    for (size_t i = 0; i < points.size(); ++i) {
        double target_depth = points[i].depth.value - interval_L.value;

        // Найти точку j, ближайшую к target_depth
        size_t j = 0;
        double min_diff = std::abs(points[0].depth.value - target_depth);

        for (size_t k = 1; k < i; ++k) {
            double diff = std::abs(points[k].depth.value - target_depth);
            if (diff < min_diff) {
                min_diff = diff;
                j = k;
            }
        }

        // Для первой точки используем её же
        if (i == 0) {
            points[i].intensity_L = 0.0;
            continue;
        }

        // Расчёт интенсивности между j и i
        points[i].intensity_L = calculateIntensityL(
            points[j].depth, points[j].inclination, points[j].magnetic_azimuth,
            points[i].depth, points[i].inclination, points[i].magnetic_azimuth,
            interval_L
        );
    }
}

WellResult processWell(
    const IntervalData& data,
    const ProcessingOptions& options,
    ProgressCallback on_progress
) {
    WellResult result;

    // Копирование метаданных
    result.uwi = data.uwi;
    result.region = data.region;
    result.field = data.field;
    result.area = data.area;
    result.cluster = data.cluster;
    result.well = data.well;
    result.rotor_table_altitude = data.rotor_table_altitude;
    result.ground_altitude = data.ground_altitude;
    result.magnetic_declination = data.magnetic_declination;
    result.target_bottom = data.target_bottom;
    result.current_bottom = data.current_bottom;
    result.azimuth_mode = options.azimuth_mode;
    result.trajectory_method = options.method;
    result.intensity_interval_L = options.intensity_interval_L;

    if (data.measurements.empty()) {
        return result;
    }

    // Прогресс
    auto reportProgress = [&on_progress](double p, std::string_view msg) {
        if (on_progress) on_progress(p, msg);
    };

    reportProgress(0.0, "Подготовка данных...");

    const size_t n = data.measurements.size();
    result.points.reserve(n);

    // Состояние траектории
    Meters x{0.0}, y{0.0}, tvd{0.0};
    AccumulatedErrors errors;

    // Обработка первой точки
    {
        const auto& m = data.measurements[0];
        ProcessedPoint pt;
        pt.depth = m.depth;
        pt.inclination = m.inclination;
        pt.magnetic_azimuth = m.magnetic_azimuth;
        pt.true_azimuth = m.true_azimuth;
        pt.rotation = m.rotation;
        pt.rop = m.rop;
        pt.marker = m.marker;

        pt.x = x;
        pt.y = y;
        pt.tvd = tvd;
        pt.absg = Meters{data.rotor_table_altitude.value - tvd.value};
        pt.shift = Meters{0.0};
        pt.direction_angle = Degrees{0.0};
        pt.elongation = Meters{0.0};
        pt.intensity_10m = 0.0;
        pt.intensity_L = 0.0;

        result.points.push_back(pt);
    }

    // Обработка остальных точек
    for (size_t i = 1; i < n; ++i) {
        if (i % 100 == 0) {
            reportProgress(0.1 + 0.6 * static_cast<double>(i) / static_cast<double>(n), "Расчёт траектории...");
        }

        const auto& prev = data.measurements[i - 1];
        const auto& curr = data.measurements[i];

        // Получить рабочие азимуты
        OptionalAngle az1 = prev.getWorkingAzimuth(options.azimuth_mode, data.magnetic_declination);
        OptionalAngle az2 = curr.getWorkingAzimuth(options.azimuth_mode, data.magnetic_declination);

        // Расчёт приращений
        auto incr = calculateIncrement(
            prev.depth, prev.inclination, az1,
            curr.depth, curr.inclination, az2,
            options.method
        );

        // Проверка вертикальности
        bool is_vertical = isEffectivelyVertical(curr, options.verticality);
        bool near_surface = isNearSurface(curr.depth, data, options.verticality);

        // Обновление TVD всегда
        tvd = Meters{tvd.value + incr.dz.value};

        // Обновление X, Y с учётом вертикальных участков
        if (!is_vertical) {
            x = Meters{x.value + incr.dx.value};
            y = Meters{y.value + incr.dy.value};
        } else if (near_surface) {
            // Приустьевая зона: X = Y = 0
            // Оставляем как есть (не добавляем приращения)
        }
        // else: глубже, X/Y фиксируются (не добавляем)

        // Расчёт интенсивности
        double int_10m = calculateIntensity10m(
            prev.depth, prev.inclination, az1,
            curr.depth, curr.inclination, az2
        );

        // Расчёт погрешностей
        if (options.calculate_errors) {
            auto err_contrib = calculateIntervalErrors(
                prev.depth, curr.depth,
                prev.inclination, curr.inclination,
                az1, az2,
                data.angle_measurement_error,
                data.azimuth_measurement_error
            );
            errors.add(err_contrib);
        }

        // Создание результата точки
        ProcessedPoint pt;
        pt.depth = curr.depth;
        pt.inclination = curr.inclination;
        pt.magnetic_azimuth = curr.magnetic_azimuth;
        pt.true_azimuth = curr.true_azimuth;
        pt.rotation = curr.rotation;
        pt.rop = curr.rop;
        pt.marker = curr.marker;

        pt.x = x;
        pt.y = y;
        pt.tvd = tvd;
        pt.absg = Meters{data.rotor_table_altitude.value - tvd.value};

        // Горизонтальное смещение и дирекционный угол
        double shift_val = std::sqrt(x.value * x.value + y.value * y.value);
        pt.shift = Meters{shift_val};

        if (shift_val > 1e-9) {
            double dir = std::atan2(y.value, x.value) * 180.0 / std::numbers::pi;
            if (dir < 0.0) dir += 360.0;
            pt.direction_angle = Degrees{dir};
        } else {
            pt.direction_angle = Degrees{0.0};
        }

        pt.elongation = Meters{curr.depth.value - tvd.value};
        pt.intensity_10m = int_10m;

        // Погрешности
        if (options.calculate_errors) {
            auto e95 = getErrors95(errors);
            pt.error_x = e95.error_x;
            pt.error_y = e95.error_y;
            pt.error_absg = e95.error_z;
            pt.error_intensity = calculateIntensityError(
                int_10m,
                data.angle_measurement_error,
                data.azimuth_measurement_error,
                Meters{curr.depth.value - prev.depth.value}
            );
        }

        result.points.push_back(pt);
    }

    reportProgress(0.75, "Расчёт интенсивности L...");

    // Расчёт INT_L
    calculateIntensityLForAllPoints(result.points, options.intensity_interval_L);

    // Сглаживание интенсивности
    if (options.smooth_intensity) {
        reportProgress(0.85, "Сглаживание интенсивности...");
        smoothIntensity(result.points, options.smoothing_window);
    }

    reportProgress(0.95, "Обновление статистики...");

    // Обновление статистики
    result.updateStatistics();

    // Интерполяция для проектных точек
    interpolateProjectPoints(result);

    reportProgress(1.0, "Обработка завершена");

    return result;
}

void interpolateProjectPoints(WellResult& result) {
    if (result.points.size() < 2) return;

    for (auto& pp : result.project_points) {
        if (!pp.isValid()) continue;

        // Определить целевую глубину
        Meters target_depth{0.0};

        if (pp.depth.has_value()) {
            target_depth = pp.depth.value();
        } else if (pp.abs_depth.has_value()) {
            // Поиск глубины по абсолютной отметке
            double target_absg = pp.abs_depth->value;

            // Бинарный поиск по ABSG (они убывают с глубиной)
            size_t lo = 0, hi = result.points.size() - 1;
            while (lo < hi) {
                size_t mid = (lo + hi) / 2;
                if (result.points[mid].absg.value > target_absg) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }

            // Интерполяция глубины
            if (lo > 0 && lo < result.points.size()) {
                const auto& p1 = result.points[lo - 1];
                const auto& p2 = result.points[lo];
                double ratio = (target_absg - p1.absg.value) / (p2.absg.value - p1.absg.value);
                target_depth = Meters{p1.depth.value + ratio * (p2.depth.value - p1.depth.value)};
            } else {
                target_depth = result.points[lo].depth;
            }
        }

        // Найти охватывающий интервал
        size_t idx = 0;
        for (size_t i = 1; i < result.points.size(); ++i) {
            if (result.points[i].depth.value >= target_depth.value) {
                idx = i;
                break;
            }
            idx = i;
        }

        if (idx == 0) {
            // Экстраполяция вверх - используем первую точку
            idx = 1;
        }

        const auto& p1 = result.points[idx - 1];
        const auto& p2 = result.points[idx];

        // Коэффициент интерполяции
        double ratio = 0.0;
        double dd = p2.depth.value - p1.depth.value;
        if (std::abs(dd) > 1e-9) {
            ratio = (target_depth.value - p1.depth.value) / dd;
        }

        // Заполнение фактических значений
        ProjectPointFactual fact;
        fact.inclination = Degrees{
            p1.inclination.value + ratio * (p2.inclination.value - p1.inclination.value)
        };
        fact.magnetic_azimuth = interpolateAzimuth(target_depth,
            p1.magnetic_azimuth, p1.depth, p2.magnetic_azimuth, p2.depth);
        fact.true_azimuth = interpolateAzimuth(target_depth,
            p1.true_azimuth, p1.depth, p2.true_azimuth, p2.depth);
        fact.x = Meters{p1.x.value + ratio * (p2.x.value - p1.x.value)};
        fact.y = Meters{p1.y.value + ratio * (p2.y.value - p1.y.value)};
        fact.tvd = Meters{p1.tvd.value + ratio * (p2.tvd.value - p1.tvd.value)};
        fact.shift = Meters{std::sqrt(fact.x.value * fact.x.value + fact.y.value * fact.y.value)};
        fact.elongation = Meters{target_depth.value - fact.tvd.value};
        fact.intensity_10m = p1.intensity_10m + ratio * (p2.intensity_10m - p1.intensity_10m);
        fact.intensity_L = p1.intensity_L + ratio * (p2.intensity_L - p1.intensity_L);

        // Расчёт отхода от проектной точки
        auto proj_coords = pp.getProjectedCoordinates();
        if (proj_coords.has_value()) {
            double dx = fact.x.value - proj_coords->first.value;
            double dy = fact.y.value - proj_coords->second.value;
            fact.deviation = Meters{std::sqrt(dx*dx + dy*dy)};

            if (fact.deviation.value > 1e-9) {
                double dir = std::atan2(dy, dx) * 180.0 / std::numbers::pi;
                if (dir < 0.0) dir += 360.0;
                fact.deviation_direction = Degrees{dir};
            }
        }

        pp.factual = fact;
    }
}

} // namespace incline::core

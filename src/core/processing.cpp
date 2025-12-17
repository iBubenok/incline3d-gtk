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
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <optional>

namespace incline::core {

namespace {

std::vector<OptionalAngle> buildWorkingAzimuths(
    const IntervalData& data,
    const ProcessingOptions& options
) {
    std::vector<OptionalAngle> azimuths;
    azimuths.reserve(data.measurements.size());

    for (const auto& m : data.measurements) {
        azimuths.push_back(m.getWorkingAzimuth(options.azimuth_mode, data.magnetic_declination));
    }

    if (options.interpolate_missing_azimuths && azimuths.size() > 1) {
        const auto& measurements = data.measurements;
        size_t idx = 0;
        while (idx < azimuths.size()) {
            if (azimuths[idx].has_value()) {
                ++idx;
                continue;
            }

            size_t start = idx;
            while (idx < azimuths.size() && !azimuths[idx].has_value()) {
                ++idx;
            }
            size_t end = idx; // первый после пробела

            // Ищем ближайший валидный слева
            std::optional<size_t> prev_valid;
            for (std::ptrdiff_t j = static_cast<std::ptrdiff_t>(start) - 1; j >= 0; --j) {
                if (azimuths[static_cast<size_t>(j)].has_value()) {
                    prev_valid = static_cast<size_t>(j);
                    break;
                }
            }

            // Ищем ближайший валидный справа
            std::optional<size_t> next_valid;
            for (size_t j = end; j < azimuths.size(); ++j) {
                if (azimuths[j].has_value()) {
                    next_valid = j;
                    break;
                }
            }

            if (prev_valid.has_value() && next_valid.has_value()) {
                const auto& prev_m = measurements[*prev_valid];
                const auto& next_m = measurements[*next_valid];
                for (size_t k = start; k < *next_valid; ++k) {
                    azimuths[k] = interpolateAzimuth(
                        measurements[k].depth,
                        azimuths[*prev_valid], prev_m.depth,
                        azimuths[*next_valid], next_m.depth
                    );
                }
            }
        }
    }

    if (options.extend_last_azimuth && !azimuths.empty()) {
        OptionalAngle last_valid;
        for (size_t i = 0; i < azimuths.size(); ++i) {
            if (azimuths[i].has_value()) {
                last_valid = azimuths[i];
            } else if (last_valid.has_value()) {
                azimuths[i] = last_valid;
            }
        }
    }

    return azimuths;
}

inline Radians doglegByMethod(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2,
    DoglegMethod method
) {
    return method == DoglegMethod::Sine
        ? calculateDoglegSin(inc1, az1, inc2, az2)
        : calculateDogleg(inc1, az1, inc2, az2);
}

} // namespace

bool isEffectivelyVertical(
    const MeasurementPoint& point,
    const VerticalityConfig& config,
    bool vertical_if_no_azimuth
) noexcept {
    // Нет азимута → вертикальная
    if (vertical_if_no_azimuth && !point.hasAzimuth()) {
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
    const std::vector<OptionalAngle>& working_azimuths,
    Meters interval_L,
    DoglegMethod dogleg_method,
    bool vertical_if_no_azimuth
) {
    if (points.size() < 2 || working_azimuths.size() != points.size()) return;

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
        OptionalAngle az1 = working_azimuths[j];
        OptionalAngle az2 = working_azimuths[i];

        if (vertical_if_no_azimuth && (!az1.has_value() || !az2.has_value())) {
            points[i].intensity_L = 0.0;
            continue;
        }

        double L = points[i].depth.value - points[j].depth.value;
        if (std::abs(L) < 1e-9) {
            points[i].intensity_L = 0.0;
            continue;
        }

        Radians dl = doglegByMethod(points[j].inclination, az1, points[i].inclination, az2, dogleg_method);
        double dl_deg = dl.toDegrees().value;
        points[i].intensity_L = (dl_deg * interval_L.value) / L;
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
    auto working_azimuths = buildWorkingAzimuths(data, options);
    result.points.reserve(n);

    // Состояние траектории
    Meters x{0.0}, y{0.0}, tvd{0.0};
    AccumulatedErrors errors;

    // Обработка первой точки
    {
        const auto& m = data.measurements[0];
        bool is_vertical = isEffectivelyVertical(m, options.verticality, options.vertical_if_no_azimuth);
        bool blank_azimuth = options.blank_vertical_azimuth && is_vertical;

        ProcessedPoint pt;
        pt.depth = m.depth;
        pt.inclination = m.inclination;
        pt.magnetic_azimuth = blank_azimuth ? std::nullopt : m.magnetic_azimuth;
        pt.true_azimuth = blank_azimuth ? std::nullopt : m.true_azimuth;
        pt.computed_azimuth = blank_azimuth ? std::nullopt : working_azimuths[0];
        pt.rotation = m.rotation;
        pt.rop = m.rop;
        pt.marker = m.marker;
        if (blank_azimuth) {
            working_azimuths[0] = std::nullopt;
        }

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

        // Получить рабочие азимуты (после интерполяции/продления)
        OptionalAngle az1 = working_azimuths[i - 1];
        OptionalAngle az2 = working_azimuths[i];

        bool prev_vertical = isEffectivelyVertical(prev, options.verticality, options.vertical_if_no_azimuth);
        bool curr_vertical = isEffectivelyVertical(curr, options.verticality, options.vertical_if_no_azimuth);
        bool missing_azimuth = options.vertical_if_no_azimuth && (!az1.has_value() || !az2.has_value());
        bool is_vertical = (prev_vertical && curr_vertical) || missing_azimuth;
        bool near_surface = isNearSurface(curr.depth, data, options.verticality);

        // Расчёт приращений
        auto incr = calculateIncrement(
            prev.depth, prev.inclination, az1,
            curr.depth, curr.inclination, az2,
            options.method
        );

        // Обновление TVD всегда
        tvd = Meters{tvd.value + incr.dz.value};

        // Обновление X, Y с учётом вертикальных участков
        if (!is_vertical) {
            x = Meters{x.value + incr.dx.value};
            y = Meters{y.value + incr.dy.value};
        } else if (near_surface) {
            // Приустьевая зона: сохраняем исходное смещение
        }

        // Расчёт интенсивности
        double int_10m = 0.0;
        if (!is_vertical) {
            Radians dl = doglegByMethod(prev.inclination, az1, curr.inclination, az2, options.dogleg_method);
            double L = curr.depth.value - prev.depth.value;
            if (std::abs(L) > 1e-6) {
                int_10m = dl.toDegrees().value * 10.0 / L;
            }
        }

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
        bool blank_azimuth = options.blank_vertical_azimuth && is_vertical;
        pt.magnetic_azimuth = blank_azimuth ? std::nullopt : curr.magnetic_azimuth;
        pt.true_azimuth = blank_azimuth ? std::nullopt : curr.true_azimuth;
        pt.computed_azimuth = blank_azimuth ? std::nullopt : az2;
        pt.rotation = curr.rotation;
        pt.rop = curr.rop;
        pt.marker = curr.marker;
        if (blank_azimuth) {
            working_azimuths[i] = std::nullopt;
        }

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
    calculateIntensityLForAllPoints(
        result.points,
        working_azimuths,
        options.intensity_interval_L,
        options.dogleg_method,
        options.vertical_if_no_azimuth
    );

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

ProcessingOptions processingOptionsFromSettings(const model::ProcessingSettings& settings) {
    ProcessingOptions options;
    options.method = settings.trajectory_method;
    options.azimuth_mode = settings.azimuth_mode;
    options.dogleg_method = settings.dogleg_method;
    options.intensity_interval_L = settings.intensity_interval_L;
    options.verticality = settings.verticality;
    options.smooth_intensity = settings.smooth_intensity;
    options.interpolate_missing_azimuths = settings.interpolate_missing_azimuths;
    options.extend_last_azimuth = settings.extend_last_azimuth;
    options.blank_vertical_azimuth = settings.blank_vertical_azimuth;
    options.vertical_if_no_azimuth = settings.vertical_if_no_azimuth;
    return options;
}

model::ProcessingSettings processingSettingsFromOptions(const ProcessingOptions& options) {
    model::ProcessingSettings settings;
    settings.trajectory_method = options.method;
    settings.azimuth_mode = options.azimuth_mode;
    settings.dogleg_method = options.dogleg_method;
    settings.intensity_interval_L = options.intensity_interval_L;
    settings.verticality = options.verticality;
    settings.smooth_intensity = options.smooth_intensity;
    settings.interpolate_missing_azimuths = options.interpolate_missing_azimuths;
    settings.extend_last_azimuth = options.extend_last_azimuth;
    settings.blank_vertical_azimuth = options.blank_vertical_azimuth;
    settings.vertical_if_no_azimuth = options.vertical_if_no_azimuth;
    return settings;
}

} // namespace incline::core

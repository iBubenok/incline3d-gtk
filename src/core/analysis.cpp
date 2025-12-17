/**
 * @file analysis.cpp
 * @brief Реализация анализа сближения и отхода
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "analysis.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace incline::core {

ProximityResult analyzeProximity(
    const WellResult& well1,
    const WellResult& well2,
    Meters /*step*/
) noexcept {
    ProximityResult result;

    if (well1.points.empty() || well2.points.empty()) {
        return result;
    }

    // Перебор всех пар точек
    for (const auto& p1 : well1.points) {
        for (const auto& p2 : well2.points) {
            double dx = p1.x.value - p2.x.value;
            double dy = p1.y.value - p2.y.value;
            double dz = p1.tvd.value - p2.tvd.value;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

            if (dist < result.min_distance.value) {
                result.min_distance = Meters{dist};
                result.depth1 = p1.depth;
                result.depth2 = p2.depth;
                result.tvd = Meters{(p1.tvd.value + p2.tvd.value) / 2.0};
                result.point1 = p1.coordinate();
                result.point2 = p2.coordinate();
            }
        }
    }

    return result;
}

ProximityResult analyzeHorizontalProximity(
    const WellResult& well1,
    const WellResult& well2,
    Meters tvd_tolerance
) noexcept {
    ProximityResult result;

    if (well1.points.empty() || well2.points.empty()) {
        return result;
    }

    for (const auto& p1 : well1.points) {
        for (const auto& p2 : well2.points) {
            // Проверяем только точки на близких TVD
            if (std::abs(p1.tvd.value - p2.tvd.value) > tvd_tolerance.value) {
                continue;
            }

            double dx = p1.x.value - p2.x.value;
            double dy = p1.y.value - p2.y.value;
            double dist = std::sqrt(dx*dx + dy*dy);

            if (dist < result.min_distance.value) {
                result.min_distance = Meters{dist};
                result.depth1 = p1.depth;
                result.depth2 = p2.depth;
                result.tvd = Meters{(p1.tvd.value + p2.tvd.value) / 2.0};
                result.point1 = p1.coordinate();
                result.point2 = p2.coordinate();
            }
        }
    }

    return result;
}

namespace {

// Интерполяция координат по TVD
Coordinate3D interpolateByTvd(const WellResult& well, Meters target_tvd) noexcept {
    if (well.points.empty()) {
        return {};
    }

    // Поиск охватывающего интервала
    size_t idx = 0;
    for (size_t i = 1; i < well.points.size(); ++i) {
        if (well.points[i].tvd.value >= target_tvd.value) {
            idx = i;
            break;
        }
        idx = i;
    }

    if (idx == 0) {
        return well.points[0].coordinate();
    }

    const auto& p1 = well.points[idx - 1];
    const auto& p2 = well.points[idx];

    double dtvd = p2.tvd.value - p1.tvd.value;
    if (std::abs(dtvd) < 1e-9) {
        return p1.coordinate();
    }

    double ratio = (target_tvd.value - p1.tvd.value) / dtvd;
    return {
        Meters{p1.x.value + ratio * (p2.x.value - p1.x.value)},
        Meters{p1.y.value + ratio * (p2.y.value - p1.y.value)},
        target_tvd
    };
}

// Интерполяция глубины MD по TVD
Meters interpolateDepthByTvd(const WellResult& well, Meters target_tvd) noexcept {
    if (well.points.empty()) {
        return Meters{0.0};
    }

    size_t idx = 0;
    for (size_t i = 1; i < well.points.size(); ++i) {
        if (well.points[i].tvd.value >= target_tvd.value) {
            idx = i;
            break;
        }
        idx = i;
    }

    if (idx == 0) {
        return well.points[0].depth;
    }

    const auto& p1 = well.points[idx - 1];
    const auto& p2 = well.points[idx];

    double dtvd = p2.tvd.value - p1.tvd.value;
    if (std::abs(dtvd) < 1e-9) {
        return p1.depth;
    }

    double ratio = (target_tvd.value - p1.tvd.value) / dtvd;
    return Meters{p1.depth.value + ratio * (p2.depth.value - p1.depth.value)};
}

} // anonymous namespace

std::vector<ProximityAtDepth> calculateProximityProfile(
    const WellResult& well1,
    const WellResult& well2,
    Meters tvd_start,
    Meters tvd_end,
    Meters step
) noexcept {
    std::vector<ProximityAtDepth> profile;

    if (well1.points.empty() || well2.points.empty()) {
        return profile;
    }

    for (double tvd = tvd_start.value; tvd <= tvd_end.value; tvd += step.value) {
        Meters target_tvd{tvd};

        auto coord1 = interpolateByTvd(well1, target_tvd);
        auto coord2 = interpolateByTvd(well2, target_tvd);

        ProximityAtDepth pad;
        pad.tvd = target_tvd;

        // Пространственное расстояние
        pad.distance_3d = coord1.distanceTo(coord2);

        // Горизонтальное расстояние
        pad.distance_horizontal = coord1.horizontalDistanceTo(coord2);

        // Глубины MD
        pad.depth1 = interpolateDepthByTvd(well1, target_tvd);
        pad.depth2 = interpolateDepthByTvd(well2, target_tvd);

        profile.push_back(pad);
    }

    return profile;
}

DeviationResult calculateDeviation(const ProjectPoint& pp) noexcept {
    DeviationResult result;

    if (!pp.factual.has_value()) {
        return result;
    }

    auto proj_coords = pp.getProjectedCoordinates();
    if (proj_coords.has_value()) {
        result.projected_x = proj_coords->first;
        result.projected_y = proj_coords->second;
    }

    result.actual_x = pp.factual->x;
    result.actual_y = pp.factual->y;

    double dx = result.actual_x.value - result.projected_x.value;
    double dy = result.actual_y.value - result.projected_y.value;

    result.distance = Meters{std::sqrt(dx*dx + dy*dy)};

    if (result.distance.value > 1e-9) {
        double dir = std::atan2(dy, dx) * 180.0 / std::numbers::pi;
        if (dir < 0.0) dir += 360.0;
        result.direction_angle = Degrees{dir};
    }

    result.within_tolerance = result.distance.value <= pp.radius.value;

    return result;
}

std::vector<Meters> calculateDeviationFromBase(
    const WellResult& well,
    const WellResult& base_well
) noexcept {
    std::vector<Meters> deviations;

    if (well.points.empty() || base_well.points.empty()) {
        return deviations;
    }

    deviations.reserve(well.points.size());

    for (const auto& pt : well.points) {
        // Найти ближайшую точку по TVD на базовой скважине
        double min_tvd_diff = std::numeric_limits<double>::max();
        size_t closest_idx = 0;

        for (size_t i = 0; i < base_well.points.size(); ++i) {
            double diff = std::abs(base_well.points[i].tvd.value - pt.tvd.value);
            if (diff < min_tvd_diff) {
                min_tvd_diff = diff;
                closest_idx = i;
            }
        }

        // Расчёт горизонтального расстояния
        const auto& base_pt = base_well.points[closest_idx];
        double dx = pt.x.value - base_pt.x.value;
        double dy = pt.y.value - base_pt.y.value;
        deviations.push_back(Meters{std::sqrt(dx*dx + dy*dy)});
    }

    return deviations;
}

DeviationStatistics calculateDeviationStatistics(const WellResult& well) noexcept {
    DeviationStatistics stats;

    stats.total_project_points = well.project_points.size();

    for (const auto& pp : well.project_points) {
        if (!pp.factual.has_value()) continue;

        if (pp.factual->deviation.value > stats.max_deviation.value) {
            stats.max_deviation = pp.factual->deviation;
            // Определить глубину
            if (pp.depth.has_value()) {
                stats.max_deviation_depth = pp.depth.value();
            }
        }

        stats.avg_deviation = Meters{
            stats.avg_deviation.value + pp.factual->deviation.value
        };

        if (pp.withinTolerance()) {
            ++stats.points_within_tolerance;
        }
    }

    if (stats.total_project_points > 0) {
        stats.avg_deviation = Meters{
            stats.avg_deviation.value / static_cast<double>(stats.total_project_points)
        };
    }

    return stats;
}

AnalysesReportData buildAnalysesReport(
    const WellResult& base_well,
    const WellResult& target_well,
    Meters profile_step
) noexcept {
    AnalysesReportData report;
    report.base_name = base_well.displayName();
    report.target_name = target_well.displayName();

    if (base_well.points.empty() || target_well.points.empty()) {
        report.valid = false;
        return report;
    }

    report.valid = true;
    report.proximity = analyzeProximity(base_well, target_well, profile_step);

    auto [base_min_tvd, base_max_tvd] = base_well.tvdRange();
    auto [target_min_tvd, target_max_tvd] = target_well.tvdRange();
    double start = std::max(base_min_tvd.value, target_min_tvd.value);
    double end = std::min(base_max_tvd.value, target_max_tvd.value);

    if (end >= start) {
        report.profile = calculateProximityProfile(
            base_well,
            target_well,
            Meters{start},
            Meters{end},
            profile_step
        );
    }

    report.deviation_stats = calculateDeviationStatistics(target_well);
    report.has_deviation = report.deviation_stats.total_project_points > 0;

    return report;
}

} // namespace incline::core

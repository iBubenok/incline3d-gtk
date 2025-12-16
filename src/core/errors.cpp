/**
 * @file errors.cpp
 * @brief Реализация расчёта погрешностей
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "errors.hpp"
#include "angle_utils.hpp"
#include <cmath>

namespace incline::core {

ErrorContribution calculateIntervalErrors(
    Meters depth1, Meters depth2,
    Degrees inc1, Degrees inc2,
    OptionalAngle az1, OptionalAngle az2,
    Degrees sigma_inc,
    Degrees sigma_az,
    Meters sigma_depth
) noexcept {
    double L = depth2.value - depth1.value;

    // Нулевой интервал - нулевые погрешности
    if (std::abs(L) < 1e-9) {
        return {0.0, 0.0, 0.0};
    }

    // Средние углы (в радианах)
    Degrees inc_avg = averageInclination(inc1, inc2);
    OptionalAngle az_avg = averageAzimuth(az1, az2);

    double theta_avg = inc_avg.toRadians().value;
    double phi_avg = az_avg.has_value() ? az_avg->toRadians().value : 0.0;

    // Погрешности в радианах
    double sigma_theta = sigma_inc.toRadians().value;
    double sigma_phi = sigma_az.toRadians().value;
    double sigma_d = sigma_depth.value;

    // === Частные производные для X ===
    // X = L * sin(θ) * cos(φ)
    double dX_dD = std::sin(theta_avg) * std::cos(phi_avg);
    double dX_dphi = -L * std::sin(theta_avg) * std::sin(phi_avg);
    double dX_dtheta = L * std::cos(theta_avg) * std::cos(phi_avg);

    // Дисперсия X
    double var_x = std::pow(dX_dD * sigma_d, 2) +
                   std::pow(dX_dphi * sigma_phi, 2) +
                   std::pow(dX_dtheta * sigma_theta, 2);
    var_x /= kVarianceDivisor;  // Эмпирический делитель

    // === Частные производные для Y ===
    // Y = L * sin(θ) * sin(φ)
    double dY_dD = std::sin(theta_avg) * std::sin(phi_avg);
    double dY_dphi = L * std::sin(theta_avg) * std::cos(phi_avg);
    double dY_dtheta = L * std::cos(theta_avg) * std::sin(phi_avg);

    // Дисперсия Y
    double var_y = std::pow(dY_dD * sigma_d, 2) +
                   std::pow(dY_dphi * sigma_phi, 2) +
                   std::pow(dY_dtheta * sigma_theta, 2);
    var_y /= kVarianceDivisor;

    // === Дисперсия Z (TVD) ===
    // Z = L * cos(θ)
    double var_z = std::pow(std::cos(theta_avg) * sigma_d, 2) +
                   std::pow(L * std::sin(theta_avg) * sigma_theta, 2);

    return {var_x, var_y, var_z};
}

Errors95 getErrors95(const AccumulatedErrors& acc) noexcept {
    return {
        Meters{std::sqrt(acc.var_x) * kConfidence95},
        Meters{std::sqrt(acc.var_y) * kConfidence95},
        Meters{std::sqrt(acc.var_z) * kConfidence95}
    };
}

double calculateIntensityError(
    [[maybe_unused]] double intensity_10m,
    Degrees sigma_inc,
    Degrees sigma_az,
    Meters interval_length
) noexcept {
    // Защита от деления на ноль
    if (interval_length.value < 1e-6) {
        return 0.0;
    }

    // Упрощённая оценка погрешности интенсивности
    // Суммарная угловая погрешность
    double sigma_total = std::sqrt(
        std::pow(sigma_inc.value, 2) + std::pow(sigma_az.value, 2)
    );

    // Масштабирование на 10м
    return (sigma_total * 10.0) / interval_length.value;
}

} // namespace incline::core

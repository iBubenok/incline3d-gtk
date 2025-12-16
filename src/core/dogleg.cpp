/**
 * @file dogleg.cpp
 * @brief Реализация расчёта пространственной интенсивности
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "dogleg.hpp"
#include "angle_utils.hpp"
#include <cmath>
#include <algorithm>

namespace incline::core {

Radians calculateDogleg(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2
) noexcept {
    // Если азимут отсутствует — только зенитная компонента
    if (!az1.has_value() || !az2.has_value()) {
        double delta_inc = std::abs(inc2.toRadians().value - inc1.toRadians().value);
        return Radians{delta_inc};
    }

    double theta1 = inc1.toRadians().value;
    double theta2 = inc2.toRadians().value;
    double phi1 = az1->toRadians().value;
    double phi2 = az2->toRadians().value;

    // cos(DL) = cos(θ2-θ1) - sin(θ1)*sin(θ2)*(1 - cos(φ2-φ1))
    double cos_DL = std::cos(theta2 - theta1) -
                    std::sin(theta1) * std::sin(theta2) * (1.0 - std::cos(phi2 - phi1));

    // Ограничение для защиты от ошибок округления
    cos_DL = std::clamp(cos_DL, -1.0, 1.0);

    return Radians{std::acos(cos_DL)};
}

Radians calculateDoglegSin(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2
) noexcept {
    // Если азимут отсутствует — только зенитная компонента
    if (!az1.has_value() || !az2.has_value()) {
        double delta_inc = std::abs(inc2.toRadians().value - inc1.toRadians().value);
        return Radians{delta_inc};
    }

    double theta1 = inc1.toRadians().value;
    double theta2 = inc2.toRadians().value;
    double phi1 = az1->toRadians().value;
    double phi2 = az2->toRadians().value;

    // sin²(DL/2) = sin²((θ2-θ1)/2) + sin²((θ2+θ1)/2) * sin²((φ2-φ1)/2)
    double half_theta_diff = (theta2 - theta1) / 2.0;
    double half_theta_sum = (theta2 + theta1) / 2.0;
    double half_phi_diff = (phi2 - phi1) / 2.0;

    double sin_half_DL = std::sqrt(
        std::pow(std::sin(half_theta_diff), 2) +
        std::pow(std::sin(half_theta_sum) * std::sin(half_phi_diff), 2)
    );

    // Ограничение
    sin_half_DL = std::clamp(sin_half_DL, -1.0, 1.0);

    return Radians{2.0 * std::asin(sin_half_DL)};
}

double calculateIntensity10m(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept {
    double L = depth2.value - depth1.value;

    // Защита от деления на ноль
    if (std::abs(L) < 1e-6) {
        return 0.0;
    }

    Radians DL = calculateDogleg(inc1, az1, inc2, az2);
    double DL_deg = DL.toDegrees().value;

    // INT10 = DL_deg * (10 / L)
    return (DL_deg * 10.0) / L;
}

double calculateIntensityL(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2,
    Meters interval_L
) noexcept {
    double L = depth2.value - depth1.value;

    // Защита от деления на ноль
    if (std::abs(L) < 1e-6) {
        return 0.0;
    }

    Radians DL = calculateDogleg(inc1, az1, inc2, az2);
    double DL_deg = DL.toDegrees().value;

    // INT_L = DL_deg * (interval_L / L)
    return (DL_deg * interval_L.value) / L;
}

double calculateZenithIntensity10m(
    Meters depth1, Degrees inc1,
    Meters depth2, Degrees inc2
) noexcept {
    double L = depth2.value - depth1.value;

    if (std::abs(L) < 1e-6) {
        return 0.0;
    }

    double delta_inc = std::abs(inc2.value - inc1.value);
    return (delta_inc * 10.0) / L;
}

double calculateAzimuthalIntensity10m(
    Meters depth1, OptionalAngle az1,
    Meters depth2, OptionalAngle az2,
    Degrees avg_inclination
) noexcept {
    if (!az1.has_value() || !az2.has_value()) {
        return 0.0;
    }

    double L = depth2.value - depth1.value;
    if (std::abs(L) < 1e-6) {
        return 0.0;
    }

    // Разность азимутов с учётом перехода 0°/360°
    Degrees az_diff = azimuthDifference(*az1, *az2);
    double delta_az = std::abs(az_diff.value);

    // Азимутальная интенсивность учитывает зенитный угол
    // При Inc=0 азимутальное изменение не даёт искривления
    double sin_inc = std::sin(avg_inclination.toRadians().value);

    return (delta_az * sin_inc * 10.0) / L;
}

} // namespace incline::core

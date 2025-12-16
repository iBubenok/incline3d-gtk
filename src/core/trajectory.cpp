/**
 * @file trajectory.cpp
 * @brief Реализация методов расчёта траектории
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "trajectory.hpp"
#include "angle_utils.hpp"
#include <cmath>
#include <algorithm>
#include <memory>

namespace incline::core {

TrajectoryIncrement averageAngle(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept {
    double L = depth2.value - depth1.value;
    if (std::abs(L) < 1e-9) {
        return {Meters{0.0}, Meters{0.0}, Meters{0.0}};
    }

    // Средние углы
    Degrees inc_avg = averageInclination(inc1, inc2);
    OptionalAngle az_avg = averageAzimuth(az1, az2);

    // В радианах
    double theta_avg = inc_avg.toRadians().value;
    double phi_avg = az_avg.has_value() ? az_avg->toRadians().value : 0.0;

    // Приращения координат
    double dX = L * std::sin(theta_avg) * std::cos(phi_avg);
    double dY = L * std::sin(theta_avg) * std::sin(phi_avg);
    double dZ = L * std::cos(theta_avg);

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}

TrajectoryIncrement balancedTangential(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept {
    double L = depth2.value - depth1.value;
    if (std::abs(L) < 1e-9) {
        return {Meters{0.0}, Meters{0.0}, Meters{0.0}};
    }

    double theta1 = inc1.toRadians().value;
    double theta2 = inc2.toRadians().value;
    double phi1 = az1.has_value() ? az1->toRadians().value : 0.0;
    double phi2 = az2.has_value() ? az2->toRadians().value : 0.0;

    // Компоненты направляющих векторов
    double dX = (L / 2.0) * (
        std::sin(theta1) * std::cos(phi1) +
        std::sin(theta2) * std::cos(phi2)
    );

    double dY = (L / 2.0) * (
        std::sin(theta1) * std::sin(phi1) +
        std::sin(theta2) * std::sin(phi2)
    );

    double dZ = (L / 2.0) * (std::cos(theta1) + std::cos(theta2));

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}

double calculateRatioFactor(Radians dogleg) noexcept {
    double DL = dogleg.value;

    // При малом dogleg RF ≈ 1
    if (std::abs(DL) < 1e-7) {
        return 1.0;
    }

    // RF = (2/DL) * tan(DL/2)
    return (2.0 / DL) * std::tan(DL / 2.0);
}

TrajectoryIncrement minimumCurvature(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept {
    double L = depth2.value - depth1.value;
    if (std::abs(L) < 1e-9) {
        return {Meters{0.0}, Meters{0.0}, Meters{0.0}};
    }

    double theta1 = inc1.toRadians().value;
    double theta2 = inc2.toRadians().value;
    double phi1 = az1.has_value() ? az1->toRadians().value : 0.0;
    double phi2 = az2.has_value() ? az2->toRadians().value : 0.0;

    // Расчёт dogleg angle (угла искривления)
    // cos(DL) = cos(θ2-θ1) - sin(θ1)*sin(θ2)*(1 - cos(φ2-φ1))
    double cos_DL = std::cos(theta2 - theta1) -
                    std::sin(theta1) * std::sin(theta2) * (1.0 - std::cos(phi2 - phi1));

    // Ограничение для защиты от ошибок округления
    cos_DL = std::clamp(cos_DL, -1.0, 1.0);
    double DL = std::acos(cos_DL);

    // Ratio Factor
    double RF = calculateRatioFactor(Radians{DL});

    // Приращения координат
    double dX = (L / 2.0) * RF * (
        std::sin(theta1) * std::cos(phi1) +
        std::sin(theta2) * std::cos(phi2)
    );

    double dY = (L / 2.0) * RF * (
        std::sin(theta1) * std::sin(phi1) +
        std::sin(theta2) * std::sin(phi2)
    );

    double dZ = (L / 2.0) * RF * (std::cos(theta1) + std::cos(theta2));

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}

TrajectoryIncrement minimumCurvatureIntegral(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept {
    // Delphi-совместимый интегральный метод минимальной кривизны
    // Реализация по unitcalc.pas:1156-1196

    double L = depth2.value - depth1.value;
    if (std::abs(L) < 1e-9) {
        return {Meters{0.0}, Meters{0.0}, Meters{0.0}};
    }

    // Углы в радианах (u1 - предыдущий, u - текущий)
    double u1 = inc1.toRadians().value;
    double u = inc2.toRadians().value;
    double a1 = az1.has_value() ? az1->toRadians().value : 0.0;
    double a = az2.has_value() ? az2->toRadians().value : 0.0;

    // Порог для сравнения углов
    constexpr double eps = 1e-9;

    // Расчёт dZ (вертикальная глубина)
    double dZ;
    if (std::abs(u - u1) > eps) {
        // u ≠ u1: интегральная формула
        dZ = L * (std::sin(u) - std::sin(u1)) / (u - u1);
    } else {
        // u = u1: прямолинейный участок
        dZ = L * std::cos(u);
    }

    // Расчёт dX, dY
    double dX = 0.0;
    double dY = 0.0;

    // Проверяем наличие азимутов
    bool has_az = az1.has_value() && az2.has_value();

    if (has_az) {
        bool u_equal = std::abs(u - u1) < eps;
        bool a_equal = std::abs(a - a1) < eps;

        if (!u_equal && !a_equal) {
            // a ≠ a1 и u ≠ u1: общий случай
            // Догleg для знаменателя (используем стандартную формулу)
            double cos_DL = std::sin(u1) * std::sin(u) * std::cos(a1 - a) +
                           std::cos(u1) * std::cos(u);
            cos_DL = std::clamp(cos_DL, -1.0, 1.0);
            double delta = std::acos(cos_DL);

            double sin_half_delta = std::sin(delta / 2.0);
            if (std::abs(sin_half_delta) > eps) {
                double factor = (std::cos(u1) - std::cos(u)) / (2.0 * (u - u1) * sin_half_delta);
                dX = L * factor * (std::sin(a) - std::sin(a1));
                dY = L * factor * (std::cos(a1) - std::cos(a));
            }
        } else if (u_equal && a_equal) {
            // a = a1 и u = u1: прямолинейный участок
            dX = L * std::sin(u) * std::cos(a);
            dY = L * std::sin(u) * std::sin(a);
        } else if (!u_equal && a_equal) {
            // u ≠ u1, a = a1: изменение только зенита
            double factor = (std::cos(u1) - std::cos(u)) / (u - u1);
            dX = L * factor * std::cos(a);
            dY = L * factor * std::sin(a);
        } else {
            // u = u1, a ≠ a1: изменение только азимута
            double factor = std::sin(u) / (a - a1);
            dX = L * factor * (std::sin(a) - std::sin(a1));
            dY = L * factor * (std::cos(a1) - std::cos(a));
        }
    }
    // Если нет азимутов, X и Y остаются нулевыми (вертикальный участок)

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}

TrajectoryIncrement ringArc(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept {
    double L = depth2.value - depth1.value;
    if (std::abs(L) < 1e-9) {
        return {Meters{0.0}, Meters{0.0}, Meters{0.0}};
    }

    double theta1 = inc1.toRadians().value;
    double theta2 = inc2.toRadians().value;
    double phi1 = az1.has_value() ? az1->toRadians().value : 0.0;
    double phi2 = az2.has_value() ? az2->toRadians().value : 0.0;

    // Особый случай: оба конца вертикальны
    if (std::abs(theta1) < 1e-7 && std::abs(theta2) < 1e-7) {
        return {Meters{0.0}, Meters{0.0}, Meters{L}};
    }

    // Особый случай: прямолинейный участок
    if (std::abs(theta1 - theta2) < 1e-7 && std::abs(phi1 - phi2) < 1e-7) {
        double dX = L * std::sin(theta1) * std::cos(phi1);
        double dY = L * std::sin(theta1) * std::sin(phi1);
        double dZ = L * std::cos(theta1);
        return {Meters{dX}, Meters{dY}, Meters{dZ}};
    }

    // Общий случай: сферическая дуга
    // Угол дуги на единичной сфере
    // cos(D) = sin(θ1)*sin(θ2)*cos(φ1-φ2) + cos(θ1)*cos(θ2)
    double cos_D = std::sin(theta1) * std::sin(theta2) * std::cos(phi1 - phi2) +
                   std::cos(theta1) * std::cos(theta2);

    cos_D = std::clamp(cos_D, -1.0, 1.0);
    double D = std::acos(cos_D);

    // Коэффициент масштабирования
    double factor;
    if (std::abs(D) < 1e-7) {
        factor = 1.0;
    } else {
        factor = std::tan(D / 2.0) / D;
    }

    // Приращения координат
    double dX = L * factor * (
        std::sin(theta1) * std::cos(phi1) +
        std::sin(theta2) * std::cos(phi2)
    );

    double dY = L * factor * (
        std::sin(theta1) * std::sin(phi1) +
        std::sin(theta2) * std::sin(phi2)
    );

    double dZ = L * factor * (std::cos(theta1) + std::cos(theta2));

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}

TrajectoryIncrement calculateIncrement(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2,
    TrajectoryMethod method
) noexcept {
    switch (method) {
        case TrajectoryMethod::AverageAngle:
            return averageAngle(depth1, inc1, az1, depth2, inc2, az2);

        case TrajectoryMethod::BalancedTangential:
            return balancedTangential(depth1, inc1, az1, depth2, inc2, az2);

        case TrajectoryMethod::MinimumCurvature:
            return minimumCurvature(depth1, inc1, az1, depth2, inc2, az2);

        case TrajectoryMethod::MinimumCurvatureIntegral:
            return minimumCurvatureIntegral(depth1, inc1, az1, depth2, inc2, az2);

        case TrajectoryMethod::RingArc:
            return ringArc(depth1, inc1, az1, depth2, inc2, az2);

        default:
            return minimumCurvature(depth1, inc1, az1, depth2, inc2, az2);
    }
}

// === Реализация калькуляторов через интерфейс ===

namespace {

class AverageAngleCalculator : public ITrajectoryCalculator {
public:
    TrajectoryIncrement calculate(
        Meters depth1, Degrees inc1, OptionalAngle az1,
        Meters depth2, Degrees inc2, OptionalAngle az2
    ) const noexcept override {
        return averageAngle(depth1, inc1, az1, depth2, inc2, az2);
    }

    TrajectoryMethod method() const noexcept override {
        return TrajectoryMethod::AverageAngle;
    }

    std::string_view name() const noexcept override {
        return "Усреднение углов";
    }
};

class BalancedTangentialCalculator : public ITrajectoryCalculator {
public:
    TrajectoryIncrement calculate(
        Meters depth1, Degrees inc1, OptionalAngle az1,
        Meters depth2, Degrees inc2, OptionalAngle az2
    ) const noexcept override {
        return balancedTangential(depth1, inc1, az1, depth2, inc2, az2);
    }

    TrajectoryMethod method() const noexcept override {
        return TrajectoryMethod::BalancedTangential;
    }

    std::string_view name() const noexcept override {
        return "Балансный тангенциальный";
    }
};

class MinimumCurvatureCalculator : public ITrajectoryCalculator {
public:
    TrajectoryIncrement calculate(
        Meters depth1, Degrees inc1, OptionalAngle az1,
        Meters depth2, Degrees inc2, OptionalAngle az2
    ) const noexcept override {
        return minimumCurvature(depth1, inc1, az1, depth2, inc2, az2);
    }

    TrajectoryMethod method() const noexcept override {
        return TrajectoryMethod::MinimumCurvature;
    }

    std::string_view name() const noexcept override {
        return "Минимальная кривизна (классич.)";
    }
};

class MinimumCurvatureIntegralCalculator : public ITrajectoryCalculator {
public:
    TrajectoryIncrement calculate(
        Meters depth1, Degrees inc1, OptionalAngle az1,
        Meters depth2, Degrees inc2, OptionalAngle az2
    ) const noexcept override {
        return minimumCurvatureIntegral(depth1, inc1, az1, depth2, inc2, az2);
    }

    TrajectoryMethod method() const noexcept override {
        return TrajectoryMethod::MinimumCurvatureIntegral;
    }

    std::string_view name() const noexcept override {
        return "Минимальная кривизна (Delphi)";
    }
};

class RingArcCalculator : public ITrajectoryCalculator {
public:
    TrajectoryIncrement calculate(
        Meters depth1, Degrees inc1, OptionalAngle az1,
        Meters depth2, Degrees inc2, OptionalAngle az2
    ) const noexcept override {
        return ringArc(depth1, inc1, az1, depth2, inc2, az2);
    }

    TrajectoryMethod method() const noexcept override {
        return TrajectoryMethod::RingArc;
    }

    std::string_view name() const noexcept override {
        return "Кольцевые дуги";
    }
};

} // anonymous namespace

std::unique_ptr<ITrajectoryCalculator> createCalculator(TrajectoryMethod method) {
    switch (method) {
        case TrajectoryMethod::AverageAngle:
            return std::make_unique<AverageAngleCalculator>();

        case TrajectoryMethod::BalancedTangential:
            return std::make_unique<BalancedTangentialCalculator>();

        case TrajectoryMethod::MinimumCurvature:
            return std::make_unique<MinimumCurvatureCalculator>();

        case TrajectoryMethod::MinimumCurvatureIntegral:
            return std::make_unique<MinimumCurvatureIntegralCalculator>();

        case TrajectoryMethod::RingArc:
            return std::make_unique<RingArcCalculator>();

        default:
            return std::make_unique<MinimumCurvatureCalculator>();
    }
}

} // namespace incline::core

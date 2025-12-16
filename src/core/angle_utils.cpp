/**
 * @file angle_utils.cpp
 * @brief Реализация утилит для работы с углами
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "angle_utils.hpp"
#include <cmath>
#include <algorithm>

namespace incline::core {

Degrees normalizeAngle(Degrees angle) noexcept {
    double a = angle.value;

    // Обработка NaN
    if (std::isnan(a)) {
        return Degrees{std::nan("")};
    }

    // Приведение к положительному диапазону
    while (a < 0.0) {
        a += 360.0;
    }
    while (a >= 360.0) {
        a -= 360.0;
    }

    // Округление 360° до 0° (с учётом погрешности)
    if (std::abs(a - 360.0) < 1e-4) {
        a = 0.0;
    }

    return Degrees{a};
}

OptionalAngle averageAzimuth(OptionalAngle a1, OptionalAngle a2) noexcept {
    // Если оба отсутствуют - возвращаем nullopt
    if (!a1.has_value() && !a2.has_value()) {
        return std::nullopt;
    }
    // Если только один отсутствует - возвращаем другой
    if (!a1.has_value()) {
        return a2;
    }
    if (!a2.has_value()) {
        return a1;
    }

    double v1 = normalizeAngle(a1.value()).value;
    double v2 = normalizeAngle(a2.value()).value;

    // Упорядочивание: v1 <= v2
    if (v1 > v2) {
        std::swap(v1, v2);
    }

    double diff_direct = v2 - v1;           // Прямая разность
    double diff_wrap = v1 + 360.0 - v2;     // Разность через 0°/360°

    double result;
    if (diff_wrap < diff_direct) {
        // Короткая дуга проходит через 0°/360°
        result = v2 + diff_wrap / 2.0;
    } else {
        // Короткая дуга не проходит через 0°/360°
        result = (v1 + v2) / 2.0;
    }

    return normalizeAngle(Degrees{result});
}

Degrees averageInclination(Degrees inc1, Degrees inc2) noexcept {
    return Degrees{(inc1.value + inc2.value) / 2.0};
}

OptionalAngle interpolateAzimuth(
    Meters target_depth,
    OptionalAngle az1, Meters depth1,
    OptionalAngle az2, Meters depth2
) noexcept {
    if (!az1.has_value() || !az2.has_value()) {
        return std::nullopt;
    }

    double d1 = depth1.value;
    double d2 = depth2.value;
    double dt = target_depth.value;

    // Защита от деления на ноль
    if (std::abs(d2 - d1) < 1e-9) {
        return az1;
    }

    double v1 = normalizeAngle(az1.value()).value;
    double v2 = normalizeAngle(az2.value()).value;

    // Обработка перехода через 0°/360°
    // Определяем, нужно ли "разворачивать" угол
    double diff = v2 - v1;
    if (diff > 180.0) {
        // v2 намного больше v1, значит v1 около 360°, v2 около 0°
        v1 += 360.0;
    } else if (diff < -180.0) {
        // v1 намного больше v2, значит v2 около 360°, v1 около 0°
        v2 += 360.0;
    }

    // Линейная интерполяция
    double ratio = (dt - d1) / (d2 - d1);
    double result = v1 + ratio * (v2 - v1);

    return normalizeAngle(Degrees{result});
}

Degrees interpolateInclination(
    Meters target_depth,
    Degrees inc1, Meters depth1,
    Degrees inc2, Meters depth2
) noexcept {
    double d1 = depth1.value;
    double d2 = depth2.value;
    double dt = target_depth.value;

    if (std::abs(d2 - d1) < 1e-9) {
        return inc1;
    }

    double ratio = (dt - d1) / (d2 - d1);
    return Degrees{inc1.value + ratio * (inc2.value - inc1.value)};
}

Degrees azimuthDifference(Degrees az1, Degrees az2) noexcept {
    double v1 = normalizeAngle(az1).value;
    double v2 = normalizeAngle(az2).value;

    double diff = v1 - v2;  // az1 - az2

    // Привести к [-180, 180)
    if (diff > 180.0) {
        diff -= 360.0;
    } else if (diff < -180.0) {
        diff += 360.0;
    }

    return Degrees{diff};
}

bool anglesClose(Degrees a1, Degrees a2, Degrees tolerance) noexcept {
    double diff = std::abs(azimuthDifference(a1, a2).value);
    return diff <= tolerance.value;
}

std::tuple<double, double, double> directionVector(
    Degrees inclination,
    OptionalAngle azimuth
) noexcept {
    double theta = inclination.toRadians().value;
    double phi = azimuth.has_value() ? azimuth->toRadians().value : 0.0;

    // X = север, Y = восток, Z = вниз
    // При Inc=0 (вертикально вниз): sin(0)=0, cos(0)=1 → (0,0,1)
    // При Inc=90, Az=0 (горизонтально на север): sin(90)cos(0)=1, sin(90)sin(0)=0, cos(90)=0 → (1,0,0)
    // При Inc=90, Az=90 (горизонтально на восток): sin(90)cos(90)=0, sin(90)sin(90)=1, cos(90)=0 → (0,1,0)
    double nx = std::sin(theta) * std::cos(phi);
    double ny = std::sin(theta) * std::sin(phi);
    double nz = std::cos(theta);

    return {nx, ny, nz};
}

} // namespace incline::core

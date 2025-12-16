/**
 * @file units.hpp
 * @brief Строго типизированные единицы измерения
 * @author Yan Bubenok <yan@bubenok.com>
 *
 * Типы-обёртки для предотвращения ошибок смешивания единиц.
 * Включает литералы для удобства: 45.0_deg, 0.5_rad, 100.0_m
 */

#pragma once

#include <cmath>
#include <compare>
#include <numbers>

namespace incline::model {

// Предварительное объявление для взаимных преобразований
struct Radians;

/**
 * @brief Угол в градусах
 */
struct Degrees {
    double value;

    constexpr explicit Degrees(double v = 0.0) noexcept : value(v) {}

    [[nodiscard]] constexpr Radians toRadians() const noexcept;

    // Арифметические операции
    constexpr Degrees operator+(Degrees other) const noexcept {
        return Degrees{value + other.value};
    }

    constexpr Degrees operator-(Degrees other) const noexcept {
        return Degrees{value - other.value};
    }

    constexpr Degrees operator*(double scalar) const noexcept {
        return Degrees{value * scalar};
    }

    constexpr Degrees operator/(double scalar) const noexcept {
        return Degrees{value / scalar};
    }

    constexpr Degrees& operator+=(Degrees other) noexcept {
        value += other.value;
        return *this;
    }

    constexpr Degrees& operator-=(Degrees other) noexcept {
        value -= other.value;
        return *this;
    }

    constexpr Degrees operator-() const noexcept {
        return Degrees{-value};
    }

    constexpr auto operator<=>(const Degrees& other) const noexcept = default;
};

/**
 * @brief Угол в радианах
 */
struct Radians {
    double value;

    constexpr explicit Radians(double v = 0.0) noexcept : value(v) {}

    [[nodiscard]] constexpr Degrees toDegrees() const noexcept {
        return Degrees{value * 180.0 / std::numbers::pi};
    }

    // Арифметические операции
    constexpr Radians operator+(Radians other) const noexcept {
        return Radians{value + other.value};
    }

    constexpr Radians operator-(Radians other) const noexcept {
        return Radians{value - other.value};
    }

    constexpr Radians operator*(double scalar) const noexcept {
        return Radians{value * scalar};
    }

    constexpr Radians operator/(double scalar) const noexcept {
        return Radians{value / scalar};
    }

    constexpr Radians operator-() const noexcept {
        return Radians{-value};
    }

    constexpr auto operator<=>(const Radians& other) const noexcept = default;
};

// Реализация toRadians после определения Radians
constexpr Radians Degrees::toRadians() const noexcept {
    return Radians{value * std::numbers::pi / 180.0};
}

/**
 * @brief Расстояние в метрах
 */
struct Meters {
    double value;

    constexpr explicit Meters(double v = 0.0) noexcept : value(v) {}

    // Арифметические операции
    constexpr Meters operator+(Meters other) const noexcept {
        return Meters{value + other.value};
    }

    constexpr Meters operator-(Meters other) const noexcept {
        return Meters{value - other.value};
    }

    constexpr Meters operator*(double scalar) const noexcept {
        return Meters{value * scalar};
    }

    constexpr Meters operator/(double scalar) const noexcept {
        return Meters{value / scalar};
    }

    constexpr double operator/(Meters other) const noexcept {
        return value / other.value;
    }

    constexpr Meters& operator+=(Meters other) noexcept {
        value += other.value;
        return *this;
    }

    constexpr Meters& operator-=(Meters other) noexcept {
        value -= other.value;
        return *this;
    }

    constexpr Meters operator-() const noexcept {
        return Meters{-value};
    }

    constexpr auto operator<=>(const Meters& other) const noexcept = default;
};

// Литералы для удобства
namespace literals {

constexpr Degrees operator""_deg(long double v) noexcept {
    return Degrees{static_cast<double>(v)};
}

constexpr Degrees operator""_deg(unsigned long long v) noexcept {
    return Degrees{static_cast<double>(v)};
}

constexpr Radians operator""_rad(long double v) noexcept {
    return Radians{static_cast<double>(v)};
}

constexpr Radians operator""_rad(unsigned long long v) noexcept {
    return Radians{static_cast<double>(v)};
}

constexpr Meters operator""_m(long double v) noexcept {
    return Meters{static_cast<double>(v)};
}

constexpr Meters operator""_m(unsigned long long v) noexcept {
    return Meters{static_cast<double>(v)};
}

} // namespace literals

// Вспомогательные функции
[[nodiscard]] inline Radians toRadians(Degrees deg) noexcept {
    return deg.toRadians();
}

[[nodiscard]] inline Degrees toDegrees(Radians rad) noexcept {
    return rad.toDegrees();
}

// Тригонометрические функции для типизированных углов
[[nodiscard]] inline double sin(Radians r) noexcept { return std::sin(r.value); }
[[nodiscard]] inline double cos(Radians r) noexcept { return std::cos(r.value); }
[[nodiscard]] inline double tan(Radians r) noexcept { return std::tan(r.value); }

[[nodiscard]] inline double sin(Degrees d) noexcept { return std::sin(d.toRadians().value); }
[[nodiscard]] inline double cos(Degrees d) noexcept { return std::cos(d.toRadians().value); }
[[nodiscard]] inline double tan(Degrees d) noexcept { return std::tan(d.toRadians().value); }

// Обратные тригонометрические функции
[[nodiscard]] inline Radians asin(double v) noexcept { return Radians{std::asin(v)}; }
[[nodiscard]] inline Radians acos(double v) noexcept { return Radians{std::acos(v)}; }
[[nodiscard]] inline Radians atan(double v) noexcept { return Radians{std::atan(v)}; }
[[nodiscard]] inline Radians atan2(double y, double x) noexcept { return Radians{std::atan2(y, x)}; }

// Абсолютное значение
[[nodiscard]] inline Degrees abs(Degrees d) noexcept { return Degrees{std::abs(d.value)}; }
[[nodiscard]] inline Radians abs(Radians r) noexcept { return Radians{std::abs(r.value)}; }
[[nodiscard]] inline Meters abs(Meters m) noexcept { return Meters{std::abs(m.value)}; }

// Квадратный корень для метров (возвращает метры для удобства)
[[nodiscard]] inline Meters sqrt(Meters m) noexcept { return Meters{std::sqrt(m.value)}; }

} // namespace incline::model

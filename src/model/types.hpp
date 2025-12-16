/**
 * @file types.hpp
 * @brief Базовые типы и перечисления
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "units.hpp"
#include <optional>
#include <string>
#include <cstdint>
#include <array>

namespace incline::model {

/**
 * @brief Опциональный угол
 *
 * ВАЖНО: std::nullopt означает отсутствие данных (вертикальный участок).
 * Значение 0° или 360° - это направление на север, НЕ отсутствие!
 */
using OptionalAngle = std::optional<Degrees>;

/**
 * @brief Режим выбора азимута для расчётов
 */
enum class AzimuthMode {
    Magnetic,   ///< Использовать магнитный азимут
    True,       ///< Использовать истинный/дирекционный азимут
    Auto        ///< Авто: истинный если задан, иначе магнитный + склонение
};

/**
 * @brief Метод расчёта траектории
 */
enum class TrajectoryMethod {
    AverageAngle,           ///< Усреднение углов (Average Angle)
    BalancedTangential,     ///< Балансный тангенциальный (Balanced Tangential)
    MinimumCurvature,       ///< Минимальная кривизна с RF (классический)
    MinimumCurvatureIntegral, ///< Минимальная кривизна интегральный (Delphi-совместимый)
    RingArc                 ///< Кольцевые дуги (Ring Arc)
};

/**
 * @brief Интерпретация магнитного склонения
 */
enum class AzimuthInterpretation {
    Geographic,   ///< Чистое склонение → географический азимут
    Directional   ///< Суммарная поправка → дирекционный угол
};

/**
 * @brief Метод расчёта dogleg (угла искривления)
 */
enum class DoglegMethod {
    Cosine,   ///< Через косинус (стандартный, как в SPE)
    Sine      ///< Через синус (как в Delphi по умолчанию)
};

/**
 * @brief Цвет в формате RGBA
 */
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    constexpr Color() noexcept = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) noexcept
        : r(r_), g(g_), b(b_), a(a_) {}

    // Предустановленные цвета
    static constexpr Color red() noexcept { return {255, 0, 0}; }
    static constexpr Color green() noexcept { return {0, 255, 0}; }
    static constexpr Color blue() noexcept { return {0, 0, 255}; }
    static constexpr Color white() noexcept { return {255, 255, 255}; }
    static constexpr Color black() noexcept { return {0, 0, 0}; }
    static constexpr Color yellow() noexcept { return {255, 255, 0}; }
    static constexpr Color cyan() noexcept { return {0, 255, 255}; }
    static constexpr Color magenta() noexcept { return {255, 0, 255}; }
    static constexpr Color lightGray() noexcept { return {192, 192, 192}; }
    static constexpr Color darkGray() noexcept { return {64, 64, 64}; }
    static constexpr Color lightBlue() noexcept { return {173, 216, 230}; }

    /**
     * @brief Парсинг из HEX-строки (#RRGGBB или #RRGGBBAA)
     */
    static Color fromHex(const std::string& hex);

    /**
     * @brief Преобразование в HEX-строку
     */
    [[nodiscard]] std::string toHex() const;

    constexpr bool operator==(const Color&) const noexcept = default;
};

/**
 * @brief 3D координата
 */
struct Coordinate3D {
    Meters x{0.0};  ///< Север (+) / Юг (-)
    Meters y{0.0};  ///< Восток (+) / Запад (-)
    Meters z{0.0};  ///< Вертикальная глубина (вниз +)

    constexpr Coordinate3D() noexcept = default;
    constexpr Coordinate3D(Meters x_, Meters y_, Meters z_) noexcept
        : x(x_), y(y_), z(z_) {}

    [[nodiscard]] constexpr Coordinate3D operator+(const Coordinate3D& other) const noexcept {
        return {x + other.x, y + other.y, z + other.z};
    }

    [[nodiscard]] constexpr Coordinate3D operator-(const Coordinate3D& other) const noexcept {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Coordinate3D& operator+=(const Coordinate3D& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    /**
     * @brief Расстояние до другой точки
     */
    [[nodiscard]] Meters distanceTo(const Coordinate3D& other) const noexcept {
        double dx = x.value - other.x.value;
        double dy = y.value - other.y.value;
        double dz = z.value - other.z.value;
        return Meters{std::sqrt(dx*dx + dy*dy + dz*dz)};
    }

    /**
     * @brief Горизонтальное расстояние до другой точки
     */
    [[nodiscard]] Meters horizontalDistanceTo(const Coordinate3D& other) const noexcept {
        double dx = x.value - other.x.value;
        double dy = y.value - other.y.value;
        return Meters{std::sqrt(dx*dx + dy*dy)};
    }
};

/**
 * @brief Приращение координат за интервал
 */
struct TrajectoryIncrement {
    Meters dx{0.0};  ///< Приращение X (север)
    Meters dy{0.0};  ///< Приращение Y (восток)
    Meters dz{0.0};  ///< Приращение TVD (вниз)
};

/**
 * @brief Конфигурация обработки вертикальных участков
 */
struct VerticalityConfig {
    Degrees critical_inclination{0.5};  ///< U_кр - критический угол
    Meters near_surface_depth{30.0};    ///< Граница приустьевой зоны по умолчанию
};

/**
 * @brief Глобальные настройки обработки
 */
struct ProcessingSettings {
    AzimuthMode azimuth_mode = AzimuthMode::Auto;
    TrajectoryMethod trajectory_method = TrajectoryMethod::MinimumCurvature;
    AzimuthInterpretation azimuth_interpretation = AzimuthInterpretation::Geographic;
    DoglegMethod dogleg_method = DoglegMethod::Sine;  ///< Метод расчёта dogleg (по умолчанию как в Delphi)
    Meters intensity_interval_L{25.0};              ///< Интервал для расчёта INT_L
    VerticalityConfig verticality;
    bool smooth_intensity = true;                   ///< Сглаживать интенсивность
};

/**
 * @brief Преобразование AzimuthMode в строку
 */
[[nodiscard]] inline std::string toString(AzimuthMode mode) {
    switch (mode) {
        case AzimuthMode::Magnetic: return "magnetic";
        case AzimuthMode::True: return "true";
        case AzimuthMode::Auto: return "auto";
    }
    return "auto";
}

/**
 * @brief Парсинг AzimuthMode из строки
 */
[[nodiscard]] inline AzimuthMode parseAzimuthMode(const std::string& str) {
    if (str == "magnetic") return AzimuthMode::Magnetic;
    if (str == "true") return AzimuthMode::True;
    return AzimuthMode::Auto;
}

/**
 * @brief Преобразование TrajectoryMethod в строку
 */
[[nodiscard]] inline std::string toString(TrajectoryMethod method) {
    switch (method) {
        case TrajectoryMethod::AverageAngle: return "average_angle";
        case TrajectoryMethod::BalancedTangential: return "balanced_tangential";
        case TrajectoryMethod::MinimumCurvature: return "minimum_curvature";
        case TrajectoryMethod::MinimumCurvatureIntegral: return "minimum_curvature_integral";
        case TrajectoryMethod::RingArc: return "ring_arc";
    }
    return "minimum_curvature";
}

/**
 * @brief Парсинг TrajectoryMethod из строки
 */
[[nodiscard]] inline TrajectoryMethod parseTrajectoryMethod(const std::string& str) {
    if (str == "average_angle") return TrajectoryMethod::AverageAngle;
    if (str == "balanced_tangential") return TrajectoryMethod::BalancedTangential;
    if (str == "minimum_curvature_integral") return TrajectoryMethod::MinimumCurvatureIntegral;
    if (str == "ring_arc") return TrajectoryMethod::RingArc;
    return TrajectoryMethod::MinimumCurvature;
}

/**
 * @brief Человекочитаемое название метода траектории
 */
[[nodiscard]] inline std::string methodDisplayName(TrajectoryMethod method) {
    switch (method) {
        case TrajectoryMethod::AverageAngle: return "Усреднение углов";
        case TrajectoryMethod::BalancedTangential: return "Балансный тангенциальный";
        case TrajectoryMethod::MinimumCurvature: return "Минимальная кривизна (классич.)";
        case TrajectoryMethod::MinimumCurvatureIntegral: return "Минимальная кривизна (Delphi)";
        case TrajectoryMethod::RingArc: return "Кольцевые дуги";
    }
    return "Минимальная кривизна (классич.)";
}

} // namespace incline::model

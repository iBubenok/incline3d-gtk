/**
 * @file shot_point.hpp
 * @brief Пункт возбуждения (для сейсмокаротажа)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "types.hpp"
#include <string>
#include <optional>
#include <vector>

namespace incline::model {

/**
 * @brief Тип маркера пункта возбуждения
 */
enum class ShotPointMarker {
    Square,    ///< Квадрат (номер "0" или "00")
    Triangle   ///< Треугольник (все остальные номера)
};

/**
 * @brief Пункт возбуждения (точка источника для сейсмокаротажа)
 *
 * Отображается только в 3D аксонометрии.
 */
struct ShotPoint {
    OptionalAngle azimuth_geographic;    ///< Географический/дирекционный азимут
    Meters shift{0.0};                   ///< Смещение от устья
    Meters ground_altitude{0.0};         ///< Альтитуда земли в точке
    std::string number;                  ///< Номер точки ("0", "00", "1", "2", ...)
    std::optional<Color> color;          ///< Цвет (если не задан — цвет скважины)

    /**
     * @brief Получить тип маркера
     *
     * Правило: "0" или "00" → квадрат, иначе → треугольник
     */
    [[nodiscard]] ShotPointMarker markerType() const noexcept {
        if (number == "0" || number == "00") {
            return ShotPointMarker::Square;
        }
        return ShotPointMarker::Triangle;
    }

    /**
     * @brief Получить координаты X, Y
     * @return Пара {X, Y} или {0, 0} если нет азимута
     */
    [[nodiscard]] std::pair<Meters, Meters> getCoordinates() const noexcept {
        if (!azimuth_geographic.has_value()) {
            return {Meters{0.0}, Meters{0.0}};
        }

        double az_rad = azimuth_geographic->value * std::numbers::pi / 180.0;
        double x = shift.value * std::cos(az_rad);
        double y = shift.value * std::sin(az_rad);
        return {Meters{x}, Meters{y}};
    }

    /**
     * @brief Получить 3D координату точки
     *
     * Z-координата равна альтитуде земли минус альтитуда стола ротора
     * (передаётся как параметр).
     */
    [[nodiscard]] Coordinate3D getCoordinate3D(Meters rotor_altitude) const noexcept {
        auto [x, y] = getCoordinates();
        // Z = 0 на уровне стола ротора, отрицательная выше
        Meters z = rotor_altitude - ground_altitude;
        return {x, y, z};
    }
};

/**
 * @brief Массив пунктов возбуждения
 */
using ShotPointList = std::vector<ShotPoint>;

} // namespace incline::model

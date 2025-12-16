/**
 * @file measurement.hpp
 * @brief Точка замера инклинометрии
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "types.hpp"
#include <vector>

namespace incline::model {

/**
 * @brief Точка замера инклинометрии (исходные данные)
 *
 * Эквивалент элемента массива ЗНАЧЕНИЯ в Delphi.
 */
struct MeasurementPoint {
    Meters depth{0.0};                    ///< Глубина по стволу (MD)
    Degrees inclination{0.0};             ///< Зенитный угол (Inc)
    OptionalAngle magnetic_azimuth;       ///< Магнитный азимут (может отсутствовать)
    OptionalAngle true_azimuth;           ///< Истинный/дирекционный азимут (может отсутствовать)

    // Дополнительные параметры замера (опциональные)
    std::optional<double> rotation;       ///< ВРАЩ - скорость вращения
    std::optional<double> rop;            ///< СКОР - скорость проходки (Rate of Penetration)
    std::optional<std::string> marker;    ///< МЕТКА - маркер/комментарий

    MeasurementPoint() = default;

    MeasurementPoint(Meters d, Degrees inc,
                     OptionalAngle mag_az = std::nullopt,
                     OptionalAngle true_az = std::nullopt)
        : depth(d), inclination(inc),
          magnetic_azimuth(mag_az), true_azimuth(true_az) {}

    /**
     * @brief Проверка наличия хотя бы одного азимута
     */
    [[nodiscard]] bool hasAzimuth() const noexcept {
        return magnetic_azimuth.has_value() || true_azimuth.has_value();
    }

    /**
     * @brief Получить рабочий азимут согласно режиму
     * @param mode Режим выбора азимута
     * @param declination Магнитное склонение (для режимов Magnetic и Auto)
     * @return Азимут для расчётов или nullopt если нет данных
     */
    [[nodiscard]] OptionalAngle getWorkingAzimuth(
        AzimuthMode mode,
        Degrees declination = Degrees{0.0}
    ) const noexcept {
        switch (mode) {
            case AzimuthMode::Magnetic:
                if (magnetic_azimuth.has_value()) {
                    return Degrees{magnetic_azimuth->value + declination.value};
                }
                return std::nullopt;

            case AzimuthMode::True:
                return true_azimuth;

            case AzimuthMode::Auto:
            default:
                // Сначала пробуем истинный, потом магнитный + склонение
                if (true_azimuth.has_value()) {
                    return true_azimuth;
                }
                if (magnetic_azimuth.has_value()) {
                    return Degrees{magnetic_azimuth->value + declination.value};
                }
                return std::nullopt;
        }
    }
};

/**
 * @brief Массив точек замеров
 */
using MeasurementList = std::vector<MeasurementPoint>;

} // namespace incline::model

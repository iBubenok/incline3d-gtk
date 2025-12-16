/**
 * @file trajectory.hpp
 * @brief Методы расчёта траектории скважины
 * @author Yan Bubenok <yan@bubenok.com>
 *
 * Реализация методов: Average Angle, Balanced Tangential,
 * Minimum Curvature, Ring Arc.
 */

#pragma once

#include "model/types.hpp"
#include "model/measurement.hpp"
#include <memory>

namespace incline::core {

using namespace incline::model;

/**
 * @brief Расчёт приращений координат методом усреднения углов (Average Angle)
 *
 * Простейший метод - используются средние значения углов на интервале.
 *
 * @param depth1 Глубина начальной точки
 * @param inc1 Зенитный угол начальной точки
 * @param az1 Азимут начальной точки
 * @param depth2 Глубина конечной точки
 * @param inc2 Зенитный угол конечной точки
 * @param az2 Азимут конечной точки
 * @return Приращения координат {dX, dY, dZ}
 */
[[nodiscard]] TrajectoryIncrement averageAngle(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт приращений координат методом балансного тангенциального (Balanced Tangential)
 *
 * Усредняются компоненты направляющих векторов в начале и конце интервала.
 */
[[nodiscard]] TrajectoryIncrement balancedTangential(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт приращений координат методом минимальной кривизны (Minimum Curvature)
 *
 * Наиболее точный и распространённый метод.
 * Траектория аппроксимируется дугой минимальной кривизны.
 */
[[nodiscard]] TrajectoryIncrement minimumCurvature(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт приращений координат методом кольцевых дуг (Ring Arc)
 *
 * Сферическая интерполяция траектории.
 *
 * Примечание: формула реализована на основе анализа эталонного кода.
 * Требуется валидация на тестовых данных.
 */
[[nodiscard]] TrajectoryIncrement ringArc(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) noexcept;

/**
 * @brief Расчёт приращений координат заданным методом
 *
 * Универсальная функция, выбирающая метод расчёта.
 */
[[nodiscard]] TrajectoryIncrement calculateIncrement(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2,
    TrajectoryMethod method
) noexcept;

/**
 * @brief Расчёт приращений между двумя точками замера
 */
[[nodiscard]] inline TrajectoryIncrement calculateIncrement(
    const MeasurementPoint& p1,
    const MeasurementPoint& p2,
    TrajectoryMethod method,
    AzimuthMode azimuth_mode,
    Degrees declination
) noexcept {
    return calculateIncrement(
        p1.depth, p1.inclination, p1.getWorkingAzimuth(azimuth_mode, declination),
        p2.depth, p2.inclination, p2.getWorkingAzimuth(azimuth_mode, declination),
        method
    );
}

/**
 * @brief Расчёт Ratio Factor для метода минимальной кривизны
 *
 * RF = (2/DL) * tan(DL/2), где DL - dogleg angle.
 * При DL ≈ 0 возвращает 1.0.
 */
[[nodiscard]] double calculateRatioFactor(Radians dogleg) noexcept;

/**
 * @brief Интерфейс калькулятора траектории для расширяемости
 */
class ITrajectoryCalculator {
public:
    virtual ~ITrajectoryCalculator() = default;

    /**
     * @brief Расчёт приращений между двумя точками
     */
    [[nodiscard]] virtual TrajectoryIncrement calculate(
        Meters depth1, Degrees inc1, OptionalAngle az1,
        Meters depth2, Degrees inc2, OptionalAngle az2
    ) const noexcept = 0;

    [[nodiscard]] virtual TrajectoryMethod method() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

/**
 * @brief Фабрика калькуляторов траектории
 */
[[nodiscard]] std::unique_ptr<ITrajectoryCalculator> createCalculator(TrajectoryMethod method);

} // namespace incline::core

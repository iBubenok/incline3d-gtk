/**
 * @file analysis.hpp
 * @brief Анализ сближения стволов и отхода
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/well_result.hpp"
#include <vector>

namespace incline::core {

using namespace incline::model;

/**
 * @brief Результат анализа сближения
 */
struct ProximityResult {
    Meters min_distance{std::numeric_limits<double>::max()};  ///< Минимальное расстояние
    Meters depth1{0.0};                  ///< Глубина на первой скважине
    Meters depth2{0.0};                  ///< Глубина на второй скважине
    Meters tvd{0.0};                     ///< TVD точки сближения
    Coordinate3D point1;                 ///< Координаты на первой скважине
    Coordinate3D point2;                 ///< Координаты на второй скважине

    [[nodiscard]] bool isValid() const noexcept {
        return min_distance.value < std::numeric_limits<double>::max() / 2.0;
    }
};

/**
 * @brief Точка сближения на определённой глубине
 */
struct ProximityAtDepth {
    Meters tvd{0.0};                     ///< TVD
    Meters distance_3d{0.0};             ///< Пространственное расстояние
    Meters distance_horizontal{0.0};     ///< Горизонтальное расстояние
    Meters depth1{0.0};                  ///< MD на первой скважине
    Meters depth2{0.0};                  ///< MD на второй скважине
};

/**
 * @brief Анализ сближения стволов двух скважин
 *
 * Находит минимальное расстояние между траекториями.
 *
 * @param well1 Результаты первой скважины
 * @param well2 Результаты второй скважины
 * @param step Шаг дискретизации по глубине (для оптимизации)
 * @return Результат анализа
 */
[[nodiscard]] ProximityResult analyzeProximity(
    const WellResult& well1,
    const WellResult& well2,
    Meters step = Meters{1.0}
) noexcept;

/**
 * @brief Анализ горизонтального сближения (только в плане)
 *
 * Сравнивает только точки на близких TVD.
 *
 * @param well1 Результаты первой скважины
 * @param well2 Результаты второй скважины
 * @param tvd_tolerance Допуск по TVD для сравнения
 * @return Результат анализа
 */
[[nodiscard]] ProximityResult analyzeHorizontalProximity(
    const WellResult& well1,
    const WellResult& well2,
    Meters tvd_tolerance = Meters{10.0}
) noexcept;

/**
 * @brief Расчёт сближения по интервалу TVD
 *
 * Возвращает массив расстояний на каждой глубине.
 *
 * @param well1 Первая скважина
 * @param well2 Вторая скважина
 * @param tvd_start Начальная TVD
 * @param tvd_end Конечная TVD
 * @param step Шаг по TVD
 * @return Массив точек сближения
 */
[[nodiscard]] std::vector<ProximityAtDepth> calculateProximityProfile(
    const WellResult& well1,
    const WellResult& well2,
    Meters tvd_start,
    Meters tvd_end,
    Meters step = Meters{10.0}
) noexcept;

/**
 * @brief Результат анализа отхода
 */
struct DeviationResult {
    Meters distance{0.0};              ///< Расстояние до проектной точки
    Degrees direction_angle{0.0};      ///< Дирекционный угол отхода
    bool within_tolerance = false;      ///< Попадание в круг допуска
    Meters projected_x{0.0};           ///< Проектная координата X
    Meters projected_y{0.0};           ///< Проектная координата Y
    Meters actual_x{0.0};              ///< Фактическая координата X
    Meters actual_y{0.0};              ///< Фактическая координата Y
};

/**
 * @brief Расчёт отхода от проектной точки
 *
 * @param pp Проектная точка (должна иметь заполненные фактические параметры)
 * @return Результат анализа
 */
[[nodiscard]] DeviationResult calculateDeviation(const ProjectPoint& pp) noexcept;

/**
 * @brief Расчёт отхода от траектории базовой скважины
 *
 * Для каждой точки текущей скважины находит ближайшую точку
 * на базовой скважине и вычисляет расстояние.
 *
 * @param well Текущая скважина
 * @param base_well Базовая скважина
 * @return Массив расстояний (по индексам точек текущей скважины)
 */
[[nodiscard]] std::vector<Meters> calculateDeviationFromBase(
    const WellResult& well,
    const WellResult& base_well
) noexcept;

/**
 * @brief Статистика отхода
 */
struct DeviationStatistics {
    Meters max_deviation{0.0};         ///< Максимальный отход
    Meters max_deviation_depth{0.0};   ///< Глубина максимального отхода
    Meters avg_deviation{0.0};         ///< Средний отход
    size_t points_within_tolerance = 0; ///< Точек в допуске
    size_t total_project_points = 0;    ///< Всего проектных точек
};

/**
 * @brief Расчёт статистики отхода для скважины
 */
[[nodiscard]] DeviationStatistics calculateDeviationStatistics(
    const WellResult& well
) noexcept;

/**
 * @brief Сводный отчёт по анализам (proximity + offset)
 */
struct AnalysesReportData {
    std::string base_name;
    std::string target_name;
    ProximityResult proximity;
    std::vector<ProximityAtDepth> profile;
    DeviationStatistics deviation_stats;
    bool has_deviation = false;
    bool valid = false;
};

/**
 * @brief Построить отчёт по анализам для двух обработанных скважин
 */
[[nodiscard]] AnalysesReportData buildAnalysesReport(
    const WellResult& base_well,
    const WellResult& target_well,
    Meters profile_step = Meters{50.0}
) noexcept;

} // namespace incline::core

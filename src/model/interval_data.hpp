/**
 * @file interval_data.hpp
 * @brief Исходные данные интервала инклинометрии
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "measurement.hpp"
#include <string>

namespace incline::model {

/**
 * @brief Исходные данные интервала инклинометрии
 *
 * Эквивалент структуры ИНТЕРВАЛЫ_ИНКЛ в Delphi.
 * Содержит метаданные скважины, параметры интервала и массив замеров.
 */
struct IntervalData {
    // === Метаданные (строки) ===
    std::string res_version;             ///< Версия формата данных
    std::string uwi;                     ///< Уникальный идентификатор скважины (UWI)
    std::string file_name;               ///< Имя исходного файла

    // Местоположение
    std::string region;                  ///< Регион
    std::string field;                   ///< Месторождение
    std::string area;                    ///< Площадь
    std::string cluster;                 ///< Куст
    std::string well;                    ///< Номер/название скважины

    // Информация об измерении
    std::string measurement_number;      ///< Номер измерения
    std::string tool;                    ///< Прибор
    std::string tool_number;             ///< Номер прибора
    std::string tool_calibration_date;   ///< Дата поверки прибора

    // Условия работ
    std::string study_type;              ///< Характер исследования
    std::string study_conditions;        ///< Условия исследования
    std::string contractor;              ///< Подрядчик
    std::string customer;                ///< Заказчик
    std::string party_chief;             ///< Начальник партии
    std::string customer_representative; ///< Представитель заказчика
    std::string study_date;              ///< Дата исследования

    // === Параметры интервала (числа) ===
    Meters interval_start{0.0};          ///< Начало интервала (MD)
    Meters interval_end{0.0};            ///< Конец интервала (MD)

    Degrees magnetic_declination{0.0};   ///< Магнитное склонение (или суммарная поправка)

    Meters rotor_table_altitude{0.0};    ///< Альтитуда стола ротора (ALTrot)
    Meters ground_altitude{0.0};         ///< Альтитуда земли

    Meters conductor_shoe{0.0};          ///< Башмак кондуктора

    Meters well_diameter{0.0};           ///< Диаметр скважины (Dскв)
    Meters casing_diameter{0.0};         ///< Диаметр колонны (Dкол)

    Meters current_bottom{0.0};          ///< Текущий забой (MD)
    Meters target_bottom{0.0};           ///< Проектный забой (MD)

    Meters allowed_bottom_deviation{0.0};///< Допустимый отход забоя

    Meters target_bottom_shift{0.0};     ///< Проектное смещение забоя
    Meters target_shift_error{0.0};      ///< Проектная ошибка смещения

    OptionalAngle target_magnetic_azimuth;  ///< Проектный магнитный азимут забоя
    OptionalAngle target_true_azimuth;      ///< Проектный истинный азимут забоя

    Degrees angle_measurement_error{0.0};   ///< Ошибка измерения зенитного угла
    Degrees azimuth_measurement_error{0.0}; ///< Ошибка измерения азимута

    // === Массив замеров ===
    MeasurementList measurements;

    /**
     * @brief Получить отображаемое имя скважины
     */
    [[nodiscard]] std::string displayName() const {
        if (!well.empty()) {
            if (!cluster.empty()) {
                return cluster + "/" + well;
            }
            return well;
        }
        if (!uwi.empty()) {
            return uwi;
        }
        if (!file_name.empty()) {
            return file_name;
        }
        return "Безымянная скважина";
    }

    /**
     * @brief Получить полное описание местоположения
     */
    [[nodiscard]] std::string locationDescription() const {
        std::string result;
        if (!region.empty()) result += region;
        if (!field.empty()) {
            if (!result.empty()) result += ", ";
            result += field;
        }
        if (!area.empty()) {
            if (!result.empty()) result += ", ";
            result += area;
        }
        if (!cluster.empty()) {
            if (!result.empty()) result += ", куст ";
            result += cluster;
        }
        return result;
    }

    /**
     * @brief Проверка наличия замеров
     */
    [[nodiscard]] bool empty() const noexcept {
        return measurements.empty();
    }

    /**
     * @brief Количество замеров
     */
    [[nodiscard]] size_t size() const noexcept {
        return measurements.size();
    }

    /**
     * @brief Получить границу приустьевой зоны
     * @param default_depth Глубина по умолчанию, если башмак кондуктора не задан
     */
    [[nodiscard]] Meters getNearSurfaceBoundary(Meters default_depth = Meters{30.0}) const noexcept {
        if (conductor_shoe.value > 0.0) {
            return conductor_shoe;
        }
        return default_depth;
    }
};

} // namespace incline::model

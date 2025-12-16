/**
 * @file validation.hpp
 * @brief Валидация данных инклинометрии
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "interval_data.hpp"
#include <string>
#include <vector>
#include <optional>

namespace incline::model {

/**
 * @brief Тип ошибки валидации
 */
enum class ValidationErrorType {
    DepthOutOfRange,        ///< Глубина вне допустимого диапазона
    InclinationOutOfRange,  ///< Зенитный угол вне диапазона
    AzimuthOutOfRange,      ///< Азимут вне диапазона
    NonMonotonicDepth,      ///< Немонотонные глубины
    IntervalMismatch,       ///< Несоответствие интервала
    MissingRequiredField,   ///< Отсутствует обязательное поле
    InvalidValue,           ///< Некорректное значение
    DuplicateDepth,         ///< Повторяющаяся глубина
    ZeroInterval            ///< Нулевой интервал между точками
};

/**
 * @brief Ошибка валидации
 */
struct ValidationError {
    ValidationErrorType type;
    std::string field;               ///< Имя поля с ошибкой
    std::string message;             ///< Описание ошибки
    std::optional<size_t> point_index; ///< Индекс точки (для ошибок в массиве)

    [[nodiscard]] std::string toString() const {
        if (point_index.has_value()) {
            return "Точка " + std::to_string(*point_index + 1) + ": " + message;
        }
        return message;
    }
};

/**
 * @brief Результат валидации
 */
struct ValidationResult {
    bool is_valid = true;
    std::vector<ValidationError> errors;
    std::vector<std::string> warnings;  ///< Некритичные замечания

    void addError(ValidationErrorType type, const std::string& field,
                  const std::string& message, std::optional<size_t> point_idx = std::nullopt) {
        is_valid = false;
        errors.push_back({type, field, message, point_idx});
    }

    void addWarning(const std::string& message) {
        warnings.push_back(message);
    }

    [[nodiscard]] bool hasErrors() const noexcept { return !errors.empty(); }
    [[nodiscard]] bool hasWarnings() const noexcept { return !warnings.empty(); }
};

/**
 * @brief Константы валидации
 */
namespace validation_limits {
    constexpr double kMinDepth = -1000.0;      ///< Мин. глубина (отриц. для шельфа)
    constexpr double kMaxDepth = 15000.0;      ///< Макс. глубина
    constexpr double kMinInclination = 0.0;    ///< Мин. зенитный угол
    constexpr double kMaxInclination = 180.0;  ///< Макс. зенитный угол
    constexpr double kMinAzimuth = 0.0;        ///< Мин. азимут
    constexpr double kMaxAzimuth = 360.0;      ///< Макс. азимут
    constexpr double kDepthTolerance = 1e-6;   ///< Толеранс для сравнения глубин
}

/**
 * @brief Валидация точки замера
 */
[[nodiscard]] inline ValidationResult validateMeasurementPoint(
    const MeasurementPoint& point,
    size_t index
) {
    ValidationResult result;

    // Проверка глубины
    if (point.depth.value < validation_limits::kMinDepth ||
        point.depth.value > validation_limits::kMaxDepth) {
        result.addError(ValidationErrorType::DepthOutOfRange, "depth",
            "Глубина " + std::to_string(point.depth.value) +
            " м вне допустимого диапазона [" +
            std::to_string(validation_limits::kMinDepth) + ", " +
            std::to_string(validation_limits::kMaxDepth) + "]", index);
    }

    // Проверка зенитного угла
    if (point.inclination.value < validation_limits::kMinInclination ||
        point.inclination.value > validation_limits::kMaxInclination) {
        result.addError(ValidationErrorType::InclinationOutOfRange, "inclination",
            "Зенитный угол " + std::to_string(point.inclination.value) +
            "° вне допустимого диапазона [0°, 180°]", index);
    }

    // Проверка магнитного азимута
    if (point.magnetic_azimuth.has_value()) {
        double az = point.magnetic_azimuth->value;
        if (az < validation_limits::kMinAzimuth || az > validation_limits::kMaxAzimuth) {
            result.addError(ValidationErrorType::AzimuthOutOfRange, "magnetic_azimuth",
                "Магнитный азимут " + std::to_string(az) +
                "° вне допустимого диапазона [0°, 360°]", index);
        }
    }

    // Проверка истинного азимута
    if (point.true_azimuth.has_value()) {
        double az = point.true_azimuth->value;
        if (az < validation_limits::kMinAzimuth || az > validation_limits::kMaxAzimuth) {
            result.addError(ValidationErrorType::AzimuthOutOfRange, "true_azimuth",
                "Истинный азимут " + std::to_string(az) +
                "° вне допустимого диапазона [0°, 360°]", index);
        }
    }

    return result;
}

/**
 * @brief Валидация интервальных данных
 */
[[nodiscard]] inline ValidationResult validateIntervalData(const IntervalData& data) {
    ValidationResult result;

    // Проверка наличия замеров
    if (data.measurements.empty()) {
        result.addError(ValidationErrorType::MissingRequiredField, "measurements",
            "Отсутствуют точки замеров");
        return result;
    }

    // Проверка интервала
    if (data.interval_end.value < data.interval_start.value) {
        result.addError(ValidationErrorType::IntervalMismatch, "interval",
            "Конец интервала (" + std::to_string(data.interval_end.value) +
            " м) меньше начала (" + std::to_string(data.interval_start.value) + " м)");
    }

    // Проверка каждой точки
    for (size_t i = 0; i < data.measurements.size(); ++i) {
        auto point_result = validateMeasurementPoint(data.measurements[i], i);
        for (const auto& err : point_result.errors) {
            result.errors.push_back(err);
        }
        if (!point_result.is_valid) {
            result.is_valid = false;
        }
    }

    // Проверка монотонности глубин
    for (size_t i = 1; i < data.measurements.size(); ++i) {
        double prev_depth = data.measurements[i-1].depth.value;
        double curr_depth = data.measurements[i].depth.value;

        if (curr_depth < prev_depth - validation_limits::kDepthTolerance) {
            result.addError(ValidationErrorType::NonMonotonicDepth, "depth",
                "Глубина точки " + std::to_string(i + 1) + " (" +
                std::to_string(curr_depth) + " м) меньше предыдущей (" +
                std::to_string(prev_depth) + " м)", i);
        }

        // Предупреждение о повторяющихся глубинах
        if (std::abs(curr_depth - prev_depth) < validation_limits::kDepthTolerance) {
            result.addWarning("Точки " + std::to_string(i) + " и " +
                std::to_string(i + 1) + " имеют одинаковую глубину");
        }
    }

    // Предупреждение об отсутствии азимутов
    size_t missing_az_count = 0;
    for (const auto& pt : data.measurements) {
        if (!pt.hasAzimuth()) {
            ++missing_az_count;
        }
    }
    if (missing_az_count > 0 && missing_az_count < data.measurements.size()) {
        result.addWarning(std::to_string(missing_az_count) +
            " точек не имеют азимута (будут считаться вертикальными)");
    }

    // Проверка соответствия глубин интервалу
    if (!data.measurements.empty()) {
        double first_depth = data.measurements.front().depth.value;
        double last_depth = data.measurements.back().depth.value;

        if (data.interval_start.value > 0.0 &&
            first_depth > data.interval_start.value + 1.0) {
            result.addWarning("Первая точка замера (" + std::to_string(first_depth) +
                " м) глубже начала интервала (" +
                std::to_string(data.interval_start.value) + " м)");
        }

        if (data.interval_end.value > 0.0 &&
            last_depth < data.interval_end.value - 1.0) {
            result.addWarning("Последняя точка замера (" + std::to_string(last_depth) +
                " м) не достигает конца интервала (" +
                std::to_string(data.interval_end.value) + " м)");
        }
    }

    return result;
}

/**
 * @brief Нормализация данных (исправление мелких проблем)
 *
 * Выполняет автокоррекцию:
 * - Нормализация азимутов к [0, 360)
 * - Сортировка по глубине при необходимости
 *
 * @return true если были внесены изменения
 */
[[nodiscard]] inline bool normalizeIntervalData(IntervalData& data) {
    bool modified = false;

    for (auto& pt : data.measurements) {
        // Нормализация азимутов
        auto normalizeAz = [&modified](OptionalAngle& az) {
            if (az.has_value()) {
                double v = az->value;
                while (v < 0.0) { v += 360.0; modified = true; }
                while (v >= 360.0) { v -= 360.0; modified = true; }
                if (std::abs(v - 360.0) < 1e-4) { v = 0.0; modified = true; }
                az = Degrees{v};
            }
        };

        normalizeAz(pt.magnetic_azimuth);
        normalizeAz(pt.true_azimuth);
    }

    // Сортировка по глубине если нужно
    for (size_t i = 1; i < data.measurements.size(); ++i) {
        if (data.measurements[i].depth.value < data.measurements[i-1].depth.value) {
            std::sort(data.measurements.begin(), data.measurements.end(),
                [](const MeasurementPoint& a, const MeasurementPoint& b) {
                    return a.depth.value < b.depth.value;
                });
            modified = true;
            break;
        }
    }

    return modified;
}

} // namespace incline::model

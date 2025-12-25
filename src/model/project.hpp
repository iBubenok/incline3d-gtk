/**
 * @file project.hpp
 * @brief Проект Incline3D (набор скважин и настройки)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "interval_data.hpp"
#include "well_result.hpp"
#include "shot_point.hpp"
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <chrono>

namespace incline::model {

/**
 * @brief Настройки 3D аксонометрии
 */
struct AxonometrySettings {
    // Камера
    float rotation_x = 30.0f;           ///< Наклон (градусы)
    float rotation_z = 45.0f;           ///< Поворот (градусы)
    float zoom = 1.0f;                  ///< Масштаб
    float pan_x = 0.0f;                 ///< Смещение X
    float pan_y = 0.0f;                 ///< Смещение Y
    float pan_z = 0.0f;                 ///< Смещение Z

    // Сетки
    bool show_grid_horizontal = true;
    bool show_grid_vertical = true;
    bool show_grid_plan = false;
    Meters grid_horizontal_depth{0.0};
    Meters grid_interval{100.0};

    // Дополнительно
    bool show_sea_level = true;
    bool show_axes = true;
    bool show_depth_labels = true;
    Meters depth_label_interval{100.0};
    bool show_well_labels = true;

    // Цвета
    Color background_color = Color::white();
    Color grid_color = Color::lightGray();
    Color sea_level_color = Color::lightBlue();

    // Толщина линий
    float trajectory_line_width = 2.0f;
    float grid_line_width = 1.0f;
};

/**
 * @brief Настройки плана (2D вид сверху)
 */
struct PlanSettings {
    // Масштаб
    float scale = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;

    // Отображение
    bool show_grid = true;
    Meters grid_interval{100.0};
    bool show_project_points = true;
    bool show_tolerance_circles = true;
    bool show_deviation_triangles = true;
    bool show_scale_bar = true;
    bool show_north_arrow = true;
    bool show_well_labels = true;
    bool show_depth_labels = false;

    // Цвета
    Color background_color = Color::white();
    Color grid_color = Color::lightGray();

    // Толщина линий
    float trajectory_line_width = 2.0f;
    float grid_line_width = 1.0f;
};

/**
 * @brief Стиль заголовка вертикальной проекции
 */
enum class HeaderStyle {
    None,       ///< Без заголовка
    Compact,    ///< Компактный
    Full        ///< Полный
};

/**
 * @brief Настройки вертикальной проекции
 */
struct VerticalProjectionSettings {
    // Плоскость
    std::optional<Degrees> plane_azimuth;  ///< nullopt = авто (по макс. смещению)
    bool auto_plane = true;

    // Масштаб
    float scale_horizontal = 1.0f;
    float scale_vertical = 1.0f;
    float pan_x = 0.0f;
    float pan_y = 0.0f;

    // Сетка
    bool show_grid = true;
    Meters grid_interval_horizontal{100.0};
    Meters grid_interval_vertical{100.0};

    // Шапка
    bool show_header = true;
    HeaderStyle header_style = HeaderStyle::Compact;

    // Дополнительно
    bool show_sea_level = true;
    bool show_depth_labels = true;
    bool show_well_labels = true;

    // Цвета
    Color background_color = Color::white();
    Color grid_color = Color::lightGray();
    Color sea_level_color = Color::lightBlue();

    // Толщина линий
    float trajectory_line_width = 2.0f;
    float grid_line_width = 1.0f;
};

/**
 * @brief Положение скважины в кусте
 */
struct ClusterPosition {
    std::variant<
        std::monostate,                          ///< Не задано
        std::pair<OptionalAngle, Meters>,        ///< Азимут + смещение
        std::pair<Meters, Meters>                ///< X, Y
    > position;

    [[nodiscard]] bool hasPosition() const noexcept {
        return !std::holds_alternative<std::monostate>(position);
    }

    /**
     * @brief Получить координаты X, Y
     */
    [[nodiscard]] std::pair<Meters, Meters> getXY() const noexcept {
        if (auto* xy = std::get_if<std::pair<Meters, Meters>>(&position)) {
            return *xy;
        }
        if (auto* az_shift = std::get_if<std::pair<OptionalAngle, Meters>>(&position)) {
            if (az_shift->first.has_value()) {
                double az_rad = az_shift->first->value * std::numbers::pi / 180.0;
                double x = az_shift->second.value * std::cos(az_rad);
                double y = az_shift->second.value * std::sin(az_rad);
                return {Meters{x}, Meters{y}};
            }
        }
        return {Meters{0.0}, Meters{0.0}};
    }
};

/**
 * @brief Запись о скважине в проекте
 */
struct WellEntry {
    std::string id;                      ///< Уникальный ID в проекте
    IntervalData source_data;            ///< Исходные данные
    std::optional<WellResult> result;    ///< Результаты обработки (если обработано)
    ProjectPointList project_points;     ///< Проектные точки (плановые данные)
    bool visible = true;                 ///< Показывать на визуализации
    bool is_base = false;                ///< Является базовой скважиной
    Color color = Color::blue();         ///< Цвет отображения

    ClusterPosition cluster_position;    ///< Положение в кусте
    ShotPointList shot_points;           ///< Пункты возбуждения

    /**
     * @brief Проверка обработанности
     */
    [[nodiscard]] bool isProcessed() const noexcept {
        return result.has_value() && !result->empty();
    }

    /**
     * @brief Получить отображаемое имя
     */
    [[nodiscard]] std::string displayName() const {
        if (result.has_value()) {
            return result->displayName();
        }
        return source_data.displayName();
    }
};

/**
 * @brief Проект Incline3D
 *
 * Содержит список скважин и настройки визуализации.
 */
struct Project {
    // === Метаданные проекта ===
    std::string name;                    ///< Название проекта
    std::string description;             ///< Описание
    std::string created_date;            ///< Дата создания (ISO 8601)
    std::string modified_date;           ///< Дата изменения (ISO 8601)
    std::string author;                  ///< Автор

    // === Скважины ===
    std::vector<WellEntry> wells;

    // === Настройки визуализации ===
    AxonometrySettings axonometry;
    PlanSettings plan;
    VerticalProjectionSettings vertical;

    // === Глобальные настройки ===
    ProcessingSettings processing;

    // === Путь к файлу (если сохранён) ===
    std::string file_path;

    /**
     * @brief Проверка на пустой проект
     */
    [[nodiscard]] bool empty() const noexcept {
        return wells.empty();
    }

    /**
     * @brief Количество скважин
     */
    [[nodiscard]] size_t size() const noexcept {
        return wells.size();
    }

    /**
     * @brief Найти скважину по ID
     */
    [[nodiscard]] WellEntry* findWell(const std::string& well_id) noexcept {
        for (auto& well : wells) {
            if (well.id == well_id) return &well;
        }
        return nullptr;
    }

    [[nodiscard]] const WellEntry* findWell(const std::string& well_id) const noexcept {
        for (const auto& well : wells) {
            if (well.id == well_id) return &well;
        }
        return nullptr;
    }

    /**
     * @brief Найти базовую скважину
     */
    [[nodiscard]] WellEntry* findBaseWell() noexcept {
        for (auto& well : wells) {
            if (well.is_base) return &well;
        }
        return nullptr;
    }

    /**
     * @brief Получить список видимых скважин
     */
    [[nodiscard]] std::vector<WellEntry*> visibleWells() noexcept {
        std::vector<WellEntry*> result;
        for (auto& well : wells) {
            if (well.visible) result.push_back(&well);
        }
        return result;
    }

    /**
     * @brief Получить список обработанных скважин
     */
    [[nodiscard]] std::vector<WellEntry*> processedWells() noexcept {
        std::vector<WellEntry*> result;
        for (auto& well : wells) {
            if (well.isProcessed()) result.push_back(&well);
        }
        return result;
    }

    /**
     * @brief Сгенерировать уникальный ID для новой скважины
     */
    [[nodiscard]] std::string generateWellId() const {
        int max_num = 0;
        for (const auto& well : wells) {
            if (well.id.starts_with("well-")) {
                try {
                    int num = std::stoi(well.id.substr(5));
                    if (num > max_num) max_num = num;
                } catch (...) {}
            }
        }
        return "well-" + std::to_string(max_num + 1);
    }

    /**
     * @brief Добавить скважину в проект
     */
    WellEntry& addWell(IntervalData data) {
        WellEntry entry;
        entry.id = generateWellId();
        entry.source_data = std::move(data);

        // Если это первая скважина, сделать её базовой
        if (wells.empty()) {
            entry.is_base = true;
        }

        wells.push_back(std::move(entry));
        return wells.back();
    }

    /**
     * @brief Удалить скважину по ID
     */
    bool removeWell(const std::string& well_id) {
        auto it = std::find_if(wells.begin(), wells.end(),
            [&](const WellEntry& w) { return w.id == well_id; });
        if (it != wells.end()) {
            wells.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Обновить дату изменения
     */
    void touch() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
        modified_date = buf;
    }

    /**
     * @brief Получить общий диапазон X для всех видимых скважин
     */
    [[nodiscard]] std::pair<Meters, Meters> totalXRange() const noexcept {
        Meters min_x{0.0}, max_x{0.0};
        bool first = true;

        for (const auto& well : wells) {
            if (!well.visible || !well.isProcessed()) continue;
            auto [wmin, wmax] = well.result->xRange();
            if (first) {
                min_x = wmin;
                max_x = wmax;
                first = false;
            } else {
                if (wmin.value < min_x.value) min_x = wmin;
                if (wmax.value > max_x.value) max_x = wmax;
            }
        }
        return {min_x, max_x};
    }

    /**
     * @brief Получить общий диапазон Y для всех видимых скважин
     */
    [[nodiscard]] std::pair<Meters, Meters> totalYRange() const noexcept {
        Meters min_y{0.0}, max_y{0.0};
        bool first = true;

        for (const auto& well : wells) {
            if (!well.visible || !well.isProcessed()) continue;
            auto [wmin, wmax] = well.result->yRange();
            if (first) {
                min_y = wmin;
                max_y = wmax;
                first = false;
            } else {
                if (wmin.value < min_y.value) min_y = wmin;
                if (wmax.value > max_y.value) max_y = wmax;
            }
        }
        return {min_y, max_y};
    }

    /**
     * @brief Получить общий диапазон TVD для всех видимых скважин
     */
    [[nodiscard]] std::pair<Meters, Meters> totalTvdRange() const noexcept {
        Meters min_tvd{0.0}, max_tvd{0.0};
        bool first = true;

        for (const auto& well : wells) {
            if (!well.visible || !well.isProcessed()) continue;
            auto [wmin, wmax] = well.result->tvdRange();
            if (first) {
                min_tvd = wmin;
                max_tvd = wmax;
                first = false;
            } else {
                if (wmin.value < min_tvd.value) min_tvd = wmin;
                if (wmax.value > max_tvd.value) max_tvd = wmax;
            }
        }
        return {min_tvd, max_tvd};
    }
};

} // namespace incline::model

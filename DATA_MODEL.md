# Модель данных Incline3D

## Обзор

Документ описывает структуры данных приложения, типы единиц измерения, семантику полей и формат хранения проектов.

## Типы единиц измерения

### Строгая типизация единиц

Для предотвращения ошибок смешивания единиц используются типы-обёртки:

```cpp
namespace incline::model {

// Углы
struct Degrees {
    double value;

    constexpr explicit Degrees(double v) : value(v) {}
    constexpr Radians toRadians() const;
};

struct Radians {
    double value;

    constexpr explicit Radians(double v) : value(v) {}
    constexpr Degrees toDegrees() const;
};

// Длины
struct Meters {
    double value;

    constexpr explicit Meters(double v) : value(v) {}
};

// Литералы для удобства
constexpr Degrees operator""_deg(long double v) { return Degrees(static_cast<double>(v)); }
constexpr Radians operator""_rad(long double v) { return Radians(static_cast<double>(v)); }
constexpr Meters operator""_m(long double v) { return Meters(static_cast<double>(v)); }

} // namespace incline::model
```

### Опциональный угол

Для представления азимута, который может отсутствовать:

```cpp
// Азимут может быть:
// - std::nullopt — отсутствует (вертикальный участок, нет данных)
// - 0.0 — азимут на север (это НЕ отсутствие!)
// - 0.0...360.0 — азимут в градусах
using OptionalAngle = std::optional<Degrees>;
```

**Важно**: значение `0°` или `360°` означает направление на север, а НЕ отсутствие данных. Отсутствие данных представляется как `std::nullopt`.

## Координатная система

### Соглашения

| Ось | Направление | Описание |
|-----|-------------|----------|
| X | +Север / −Юг | Смещение по оси север-юг |
| Y | +Восток / −Запад | Смещение по оси восток-запад |
| TVD | +Вниз | Вертикальная глубина (True Vertical Depth) |

### Углы

| Параметр | Диапазон | Описание |
|----------|----------|----------|
| Зенитный угол (Inc) | 0°...180° | 0° = вертикально вниз, 90° = горизонтально |
| Азимут (Az) | 0°...360° | 0°/360° = север, 90° = восток, 180° = юг, 270° = запад |

### Абсолютная отметка

```
ABSG = ALTrot − TVD
```

Где:
- `ABSG` — абсолютная отметка глубины (положительно над уровнем моря, отрицательно — под)
- `ALTrot` — альтитуда стола ротора (высота устья над уровнем моря)
- `TVD` — вертикальная глубина от устья

## Структуры данных

### IntervalData (ИНТЕРВАЛЫ_ИНКЛ)

Исходные данные интервала инклинометрии — входные данные для обработки.

```cpp
struct IntervalData {
    // === Метаданные (строки) ===
    std::string res_version;             // Версия формата данных
    std::string uwi;                     // Уникальный идентификатор скважины (UWI)
    std::string file_name;               // Имя исходного файла

    // Местоположение
    std::string region;                  // Регион
    std::string field;                   // Месторождение
    std::string area;                    // Площадь
    std::string cluster;                 // Куст
    std::string well;                    // Номер/название скважины

    // Информация об измерении
    std::string measurement_number;      // Номер измерения
    std::string tool;                    // Прибор
    std::string tool_number;             // Номер прибора
    std::string tool_calibration_date;   // Дата поверки прибора

    // Условия работ
    std::string study_type;              // Характер исследования
    std::string study_conditions;        // Условия исследования
    std::string contractor;              // Подрядчик
    std::string customer;                // Заказчик
    std::string party_chief;             // Начальник партии
    std::string customer_representative; // Представитель заказчика
    std::string study_date;              // Дата исследования

    // === Параметры интервала (числа) ===
    Meters interval_start;               // Начало интервала (MD)
    Meters interval_end;                 // Конец интервала (MD)

    Degrees magnetic_declination;        // Магнитное склонение (или суммарная поправка)

    Meters rotor_table_altitude;         // Альтитуда стола ротора (ALTrot)
    Meters ground_altitude;              // Альтитуда земли

    Meters conductor_shoe;               // Башмак кондуктора

    Meters well_diameter;                // Диаметр скважины (Dскв)
    Meters casing_diameter;              // Диаметр колонны (Dкол)

    Meters current_bottom;               // Текущий забой (MD)
    Meters target_bottom;                // Проектный забой (MD)

    Meters allowed_bottom_deviation;     // Допустимый отход забоя

    Meters target_bottom_shift;          // Проектное смещение забоя
    Meters target_shift_error;           // Проектная ошибка смещения

    OptionalAngle target_magnetic_azimuth; // Проектный магнитный азимут забоя
    OptionalAngle target_true_azimuth;     // Проектный истинный азимут забоя

    Degrees angle_measurement_error;     // Ошибка измерения зенитного угла
    Degrees azimuth_measurement_error;   // Ошибка измерения азимута

    // === Массив замеров ===
    std::vector<MeasurementPoint> measurements;
};
```

### MeasurementPoint (ЗНАЧЕНИЯ)

Одна точка замера (исходные данные):

```cpp
struct MeasurementPoint {
    Meters depth;                        // Глубина по стволу (MD)
    Degrees inclination;                 // Зенитный угол (Inc)
    OptionalAngle magnetic_azimuth;      // Магнитный азимут (может отсутствовать)
    OptionalAngle true_azimuth;          // Истинный/дирекционный азимут (может отсутствовать)
};
```

### WellResult (РЕЗ_ИНКЛ)

Результаты обработки скважины:

```cpp
struct WellResult {
    // === Идентификация (копируется из IntervalData) ===
    std::string uwi;
    std::string region;
    std::string field;
    std::string area;
    std::string cluster;
    std::string well;

    // === Параметры (копируются/рассчитываются) ===
    Meters rotor_table_altitude;
    Meters ground_altitude;
    Degrees magnetic_declination;
    Meters target_bottom;
    Meters current_bottom;

    // === Фактические значения забоя (рассчитанные) ===
    Meters actual_shift;                 // Фактическое смещение забоя
    Meters shift_deviation;              // Отклонение по смещению

    OptionalAngle actual_bottom_azimuth; // Фактический азимут забоя
    Degrees actual_direction_angle;      // Фактический дирекционный угол смещения
    Degrees direction_angle_deviation;   // Отклонение по дир. углу

    Meters actual_bottom_deviation;      // Фактический отход забоя
    OptionalAngle deviation_azimuth;     // Азимут отхода забоя
    OptionalAngle true_deviation_azimuth;// Истинный азимут отхода

    Meters target_abs_bottom;            // Проектная абсолютная отметка забоя
    Meters actual_abs_bottom;            // Фактическая абсолютная отметка забоя

    // === Положение в кусте ===
    Meters cluster_shift;                // Смещение в кусте
    OptionalAngle cluster_shift_azimuth; // Азимут смещения в кусте

    // === Максимумы ===
    Degrees max_inclination;             // Макс. зенитный угол
    Meters max_inclination_depth;        // Глубина макс. зенитного угла

    double max_intensity_10m;            // Макс. интенсивность на 10м (°/10м)
    Meters max_intensity_10m_depth;      // Глубина макс. интенсивности 10м

    double max_intensity_L;              // Макс. интенсивность на L м
    Meters max_intensity_L_depth;        // Глубина макс. интенсивности L
    Meters intensity_interval_L;         // Интервал L для расчёта интенсивности

    // === Настройки обработки ===
    AzimuthMode azimuth_mode;            // Режим азимута для расчёта
    TrajectoryMethod trajectory_method;  // Метод расчёта траектории

    // === Результаты по точкам ===
    std::vector<ProcessedPoint> points;

    // === Проектные точки ===
    std::vector<ProjectPoint> project_points;
};
```

### ProcessedPoint (РЕЗ_ОБР_ИНКЛ)

Результат обработки одной точки:

```cpp
struct ProcessedPoint {
    // === Исходные данные (копия) ===
    Meters depth;                        // Глубина MD
    Degrees inclination;                 // Зенитный угол
    OptionalAngle magnetic_azimuth;      // Магнитный азимут
    OptionalAngle true_azimuth;          // Истинный азимут

    // === Дополнительные параметры замера (если есть) ===
    std::optional<double> rotation;      // ВРАЩ — скорость вращения
    std::optional<double> rop;           // СКОР — скорость проходки
    std::optional<std::string> marker;   // МЕТК — метка/маркер

    // === Рассчитанные величины ===
    Meters elongation;                   // УДЛГ — удлинение (MD − TVD)
    Meters shift;                        // СМГ — горизонтальное смещение от устья
    Degrees direction_angle;             // ДУГ — дирекционный угол смещения

    Meters x;                            // X — смещение на север
    Meters y;                            // Y — смещение на восток
    Meters tvd;                          // Вертикальная глубина
    Meters absg;                         // Абсолютная отметка

    double intensity_10m;                // ИНТГ — интенсивность на 10м (°/10м)
    double intensity_L;                  // ИНТГ2 — интенсивность на L м (°/L м)

    // === Погрешности ===
    Meters error_x;                      // ОШ_СМ_X — погрешность X
    Meters error_y;                      // ОШ_СМ_Y — погрешность Y
    Meters error_absg;                   // ОШ_АБСГ — погрешность абс. отметки
    double error_intensity;              // ОШ_ИНТГ — погрешность интенсивности

    // === Плановые значения (если заданы) ===
    std::optional<Meters> planned_tvd;   // Верт_план
    std::optional<Meters> planned_x;     // X_план
    std::optional<Meters> planned_y;     // Y_план
    std::optional<double> planned_intensity_10m;  // ИНТГ_план
    std::optional<double> planned_intensity_L;    // ИНТГ2_план
};
```

### ProjectPoint (Project_Points)

Проектная точка — целевой пласт или контрольная точка:

```cpp
struct ProjectPoint {
    // === Идентификация ===
    std::string name;                    // PlastName — название пласта

    // === Проектные параметры ===
    OptionalAngle azimuth_geographic;    // AzimGeogr — географический азимут
    Meters shift;                        // Shift — проектное смещение

    // Глубина задаётся ОДНИМ из способов:
    std::optional<Meters> depth;         // Depth — глубина по стволу (MD)
    std::optional<Meters> abs_depth;     // AbsDepth — абсолютная отметка

    Meters radius;                       // Radius — радиус допуска (круг на плане)

    // === Базовая точка (если проектная точка задана относительно) ===
    std::optional<Meters> base_shift;           // BaseShift
    std::optional<OptionalAngle> base_azimuth;  // BaseAzimGeogr
    std::optional<Meters> base_depth;           // BaseDepth

    // === Фактические параметры (рассчитываются при обработке) ===
    struct Factual {
        Degrees inclination;             // Фактический зенитный угол
        OptionalAngle magnetic_azimuth;  // Фактический магнитный азимут
        OptionalAngle true_azimuth;      // Фактический истинный азимут
        Meters shift;                    // Фактическое смещение
        Meters elongation;               // Фактическое удлинение
        Meters x;                        // Фактическая координата X
        Meters y;                        // Фактическая координата Y
        Meters deviation;                // Отход от проектной точки
        Degrees deviation_direction;     // Дирекционный угол отхода
        Meters tvd;                      // Фактическая вертикальная глубина
        double intensity_10m;            // Интенсивность на 10м
        double intensity_L;              // Интенсивность на L м
    };
    std::optional<Factual> factual;      // Заполняется после обработки
};
```

### ShotPoint (Пункт возбуждения)

Точка возбуждения для сейсмокаротажа:

```cpp
struct ShotPoint {
    OptionalAngle azimuth_geographic;    // Географический/дирекционный азимут
    Meters shift;                        // Смещение от устья
    Meters ground_altitude;              // Альтитуда земли в точке
    std::string number;                  // Номер точки ("0", "00", "1", "2", ...)
    std::optional<Color> color;          // Цвет (если не задан — использовать цвет скважины)

    // Правила отображения:
    // - number == "0" или "00" → квадрат
    // - иначе → треугольник
    // - Отображается только в 3D аксонометрии
};
```

### Project (Проект)

Проект объединяет скважины и настройки визуализации:

```cpp
struct Project {
    // === Метаданные проекта ===
    std::string name;                    // Название проекта
    std::string description;             // Описание
    std::string created_date;            // Дата создания
    std::string modified_date;           // Дата изменения

    // === Скважины ===
    std::vector<WellEntry> wells;

    // === Настройки визуализации ===
    AxonometrySettings axonometry;
    PlanSettings plan;
    VerticalProjectionSettings vertical;

    // === Глобальные настройки ===
    GlobalSettings global;
};

struct WellEntry {
    std::string id;                      // Уникальный ID в проекте
    IntervalData source_data;            // Исходные данные
    std::optional<WellResult> result;    // Результаты обработки (если обработано)
    bool visible;                        // Показывать на визуализации
    bool is_base;                        // Является базовой скважиной
    Color color;                         // Цвет отображения

    // Положение в кусте (альтернативные способы задания)
    std::variant<
        std::monostate,                  // Не задано
        std::pair<OptionalAngle, Meters>,// Азимут + смещение
        std::pair<Meters, Meters>        // X, Y
    > cluster_position;

    // Пункты возбуждения
    std::vector<ShotPoint> shot_points;
};
```

## Перечисления

### AzimuthMode

Режим выбора азимута для расчётов:

```cpp
enum class AzimuthMode {
    Magnetic,   // Использовать магнитный азимут
    True,       // Использовать истинный/дирекционный азимут
    Auto        // Авто: истинный если задан, иначе магнитный
};
```

**Режим Auto** (по умолчанию):
1. Если для точки задан истинный азимут — использовать его
2. Иначе, если задан магнитный — прибавить магнитное склонение
3. Если ни один не задан — точка считается вертикальной

### TrajectoryMethod

Метод расчёта траектории:

```cpp
enum class TrajectoryMethod {
    AverageAngle,       // Усреднение углов (Average Angle)
    BalancedTangential, // Балансный тангенциальный (Balanced Tangential)
    MinimumCurvature,   // Минимальная кривизна (Minimum Curvature)
    RingArc             // Кольцевые дуги (Ring Arc)
};
```

## Семантика азимутов и вертикальных участков

### Азимут 0° vs отсутствие азимута

| Значение | Представление | Семантика |
|----------|--------------|-----------|
| 0° или 360° | `OptionalAngle(Degrees(0.0))` | Направление на север |
| Отсутствует | `std::nullopt` | Нет данных (вертикальный участок) |

### Обработка вертикальных участков

Критический угол `U_кр` (Critical Inclination) определяет порог вертикальности:

```cpp
struct VerticalityConfig {
    Degrees critical_inclination;        // U_кр, обычно 0.25° - 1.0°
    Meters near_surface_depth;           // Глубина "приустьевой зоны"
};
```

**Правила**:

1. **Приустьевая зона** (глубина ≤ `conductor_shoe` или `near_surface_depth`):
   - Если `Inc ≤ U_кр`: X = 0, Y = 0

2. **Глубже приустьевой зоны**:
   - Если `Inc ≤ U_кр`: X и Y фиксируются на значениях предыдущей точки

**Допущение**: граница приустьевой зоны определяется как `conductor_shoe` (башмак кондуктора). Если не задан — используется `near_surface_depth` из настроек (по умолчанию 30 м).

> **Как проверить**: сравнить результаты с Delphi на скважинах с вертикальными участками в начале и середине траектории.

## Суммарная поправка и сближение меридианов

### Режимы интерпретации магнитного склонения

Поле `magnetic_declination` может интерпретироваться двумя способами:

#### Режим A: Чистое магнитное склонение

```
Азимут_географический = Азимут_магнитный + Маг_склонение
```

В этом режиме `true_azimuth` — географический азимут.

#### Режим B: Суммарная поправка

```
Суммарная_поправка = Маг_склонение − Сближение_меридианов
Азимут_дирекционный = Азимут_магнитный + Суммарная_поправка
```

В этом режиме `true_azimuth` — дирекционный угол (в проекции Гаусса-Крюгера).

### Отображение в UI

```cpp
enum class AzimuthInterpretation {
    Geographic,   // Режим A: показывать как "географический азимут"
    Directional   // Режим B: показывать как "дирекционный угол"
};
```

UI должен корректно отображать подписи в зависимости от режима:
- Режим A: "Азимут магн.", "Азимут геогр."
- Режим B: "Азимут магн.", "Дир. угол"

## Формат файла проекта (*.inclproj)

### Структура JSON

```json
{
  "version": "1.0.0",
  "format": "incline3d-project",

  "metadata": {
    "name": "Куст 123",
    "description": "Инклинометрия куста 123 месторождения X",
    "created": "2024-01-15T10:30:00Z",
    "modified": "2024-01-16T14:20:00Z",
    "author": "Иванов И.И."
  },

  "wells": [
    {
      "id": "well-001",
      "visible": true,
      "is_base": true,
      "color": "#FF0000",
      "cluster_position": {
        "type": "azimuth_shift",
        "azimuth": 45.0,
        "shift": 10.0
      },

      "source_data": {
        "uwi": "123456789012",
        "region": "Западная Сибирь",
        "field": "Приобское",
        "area": "Северный участок",
        "cluster": "123",
        "well": "123-1",

        "interval_start": 0.0,
        "interval_end": 2500.0,
        "magnetic_declination": 12.5,
        "rotor_table_altitude": 85.3,
        "ground_altitude": 80.1,
        "conductor_shoe": 30.0,

        "measurements": [
          {"depth": 0.0, "inclination": 0.0, "magnetic_azimuth": null, "true_azimuth": null},
          {"depth": 100.0, "inclination": 2.5, "magnetic_azimuth": 45.0, "true_azimuth": null},
          {"depth": 200.0, "inclination": 5.0, "magnetic_azimuth": 47.0, "true_azimuth": null}
        ]
      },

      "result": {
        "points": [
          {
            "depth": 0.0,
            "inclination": 0.0,
            "x": 0.0,
            "y": 0.0,
            "tvd": 0.0,
            "absg": 85.3,
            "intensity_10m": 0.0
          }
        ],
        "max_inclination": 45.0,
        "max_inclination_depth": 1500.0
      },

      "project_points": [
        {
          "name": "Пласт БС10",
          "azimuth_geographic": 60.0,
          "shift": 500.0,
          "abs_depth": -2200.0,
          "radius": 50.0
        }
      ],

      "shot_points": [
        {
          "azimuth_geographic": 90.0,
          "shift": 100.0,
          "ground_altitude": 80.0,
          "number": "1",
          "color": "#00FF00"
        }
      ]
    }
  ],

  "settings": {
    "global": {
      "azimuth_mode": "auto",
      "trajectory_method": "minimum_curvature",
      "intensity_interval_L": 25.0,
      "critical_inclination": 0.5,
      "azimuth_interpretation": "geographic"
    },

    "axonometry": {
      "scale": 1.0,
      "rotation_x": 30.0,
      "rotation_z": 45.0,
      "show_grid_horizontal": true,
      "show_grid_vertical": true,
      "show_grid_plan": false,
      "show_sea_level": true,
      "show_axes": true,
      "depth_labels_interval": 100.0,
      "background_color": "#FFFFFF"
    },

    "plan": {
      "scale": 1.0,
      "show_grid": true,
      "show_project_points": true,
      "show_tolerance_circles": true,
      "show_deviation_triangles": true,
      "grid_interval": 100.0
    },

    "vertical": {
      "scale_horizontal": 1.0,
      "scale_vertical": 1.0,
      "plane_azimuth": null,
      "show_grid": true,
      "show_header": true,
      "grid_interval_horizontal": 100.0,
      "grid_interval_vertical": 100.0
    }
  }
}
```

### Правила сериализации

1. **Версионирование**: поле `version` позволяет поддерживать миграции при изменении формата
2. **Числа**: все числа хранятся как JSON numbers (double)
3. **Отсутствующие значения**: `null` для `OptionalAngle` и других optional-типов
4. **Даты**: ISO 8601 формат
5. **Цвета**: шестнадцатеричный формат `#RRGGBB`
6. **Кодировка**: UTF-8

### Миграция версий

```cpp
class ProjectMigrator {
public:
    Project migrate(const json& data);

private:
    Project migrateFrom_0_9(const json& data);  // 0.9.x → 1.0.0
    Project migrateFrom_1_0(const json& data);  // 1.0.x → 1.1.0 (будущее)
};
```

## Валидация данных

### Правила валидации MeasurementPoint

```cpp
struct MeasurementValidation {
    static constexpr Meters kMinDepth{-1000.0};      // Мин. глубина (отриц. для шельфа)
    static constexpr Meters kMaxDepth{15000.0};      // Макс. глубина
    static constexpr Degrees kMinInclination{0.0};   // Мин. зенитный угол
    static constexpr Degrees kMaxInclination{180.0}; // Макс. зенитный угол
    static constexpr Degrees kMinAzimuth{0.0};       // Мин. азимут
    static constexpr Degrees kMaxAzimuth{360.0};     // Макс. азимут

    static ValidationResult validate(const MeasurementPoint& point);
};
```

### Правила валидации IntervalData

1. `interval_start` < `interval_end`
2. Глубины замеров в пределах интервала
3. Глубины замеров монотонно возрастают (или как минимум не убывают)
4. Зенитные углы в допустимом диапазоне
5. Азимуты нормализованы к [0, 360)

### Типы ошибок валидации

```cpp
enum class ValidationErrorType {
    DepthOutOfRange,
    InclinationOutOfRange,
    AzimuthOutOfRange,
    NonMonotonicDepth,
    IntervalMismatch,
    MissingRequiredField,
    InvalidValue
};

struct ValidationError {
    ValidationErrorType type;
    std::string field;
    std::string message;
    std::optional<size_t> point_index;  // Для ошибок в массиве замеров
};

struct ValidationResult {
    bool is_valid;
    std::vector<ValidationError> errors;
    std::vector<std::string> warnings;  // Не критичные замечания
};
```

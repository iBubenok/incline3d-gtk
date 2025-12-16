# Форматы файлов Incline3D

## Обзор

Документ описывает поддерживаемые форматы файлов для импорта и экспорта данных инклинометрии.

## Сводка поддерживаемых форматов

| Формат | Расширение | Импорт | Экспорт | Описание |
|--------|------------|--------|---------|----------|
| Проект Incline3D | .inclproj | ✓ | ✓ | Собственный формат проекта |
| CSV | .csv, .txt | ✓ | ✓ | Текстовые данные с разделителями |
| LAS 2.0 | .las | ✓ | ✓ | Log ASCII Standard |
| ZAK | .zak | ✓ | — | Формат заключений |
| Шаблоны экспорта | .mkl, .mkz | — | ✓ | Шаблонизированный экспорт |

---

## 1. Формат проекта (*.inclproj)

### 1.1. Описание

Собственный формат для хранения полного состояния проекта: скважины, результаты обработки, проектные точки, пункты возбуждения и настройки визуализации.

### 1.2. Структура

Формат: JSON с версионированием.

```json
{
  "version": "1.0.0",
  "format": "incline3d-project",
  "metadata": { ... },
  "wells": [ ... ],
  "settings": { ... }
}
```

Полная спецификация — см. [DATA_MODEL.md](DATA_MODEL.md#формат-файла-проекта-inclproj).

### 1.3. Версионирование

| Версия | Изменения |
|--------|-----------|
| 1.0.0 | Начальная версия |

При чтении файла старой версии выполняется автоматическая миграция.

### 1.4. Чтение и запись

```cpp
// Чтение
Project loadProject(const std::filesystem::path& path);

// Запись (атомарная)
void saveProject(const Project& project, const std::filesystem::path& path);
```

---

## 2. CSV (Comma-Separated Values)

### 2.1. Описание

Универсальный текстовый формат для обмена табличными данными. Поддерживается гибкая настройка разделителей и маппинга полей.

### 2.2. Поддерживаемые варианты

| Параметр | Варианты |
|----------|----------|
| Разделитель | `;` (по умолч.), `,`, `\t`, `|` |
| Кодировка | UTF-8 (по умолч.), Windows-1251, CP866 |
| Заголовок | Первая строка / Без заголовка |
| Десятичный разделитель | `.` (по умолч.), `,` |

### 2.3. Структура файла замеров

#### Минимальный набор колонок

```csv
Глубина;Угол;Азимут
0.0;0.0;
100.0;2.5;45.0
200.0;5.0;47.0
300.0;8.2;50.5
```

#### Расширенный набор

```csv
Глубина;Угол;Азимут_магн;Азимут_истинный;ВРАЩ;СКОР;МЕТКА
0.0;0.0;;;;
100.0;2.5;45.0;57.5;120;15.2;
200.0;5.0;47.0;59.5;118;14.8;
```

### 2.4. Маппинг полей при импорте

При импорте пользователь настраивает соответствие колонок файла полям данных:

```cpp
struct CsvFieldMapping {
    std::optional<size_t> depth_column;           // Обязательно
    std::optional<size_t> inclination_column;     // Обязательно
    std::optional<size_t> magnetic_azimuth_column;
    std::optional<size_t> true_azimuth_column;
    std::optional<size_t> rotation_column;        // ВРАЩ
    std::optional<size_t> rop_column;             // СКОР
    std::optional<size_t> marker_column;          // МЕТКА
};
```

### 2.5. Автоопределение формата

Система пытается автоматически определить:
1. Разделитель (по частоте символов)
2. Наличие заголовка (по первой строке)
3. Соответствие колонок (по названиям заголовков)

#### Распознаваемые имена колонок

| Поле | Варианты названий |
|------|-------------------|
| Глубина | `Глубина`, `Depth`, `MD`, `DEPT`, `ГЛ` |
| Зенитный угол | `Угол`, `Inclination`, `Inc`, `INC`, `INCL`, `Зенит` |
| Магнитный азимут | `Азимут`, `Azimuth`, `Az`, `AZI`, `AZIM`, `Азимут_магн` |
| Истинный азимут | `Азимут_истинный`, `True_Azimuth`, `AZ_TRUE`, `Азимут_геогр` |

### 2.6. Экспорт в CSV

#### Параметры экспорта

```cpp
struct CsvExportOptions {
    char delimiter = ';';
    std::string encoding = "UTF-8";
    bool include_header = true;
    char decimal_separator = '.';

    std::vector<ExportField> fields;  // Какие поля экспортировать
};

enum class ExportField {
    Depth,
    Inclination,
    MagneticAzimuth,
    TrueAzimuth,
    X,
    Y,
    TVD,
    ABSG,
    Shift,
    DirectionAngle,
    Elongation,
    Intensity10m,
    IntensityL,
    ErrorX,
    ErrorY,
    ErrorABSG
};
```

#### Пример экспорта результатов

```csv
Глубина;Угол;Азимут;X;Y;TVD;АБСГ;Смещ;ДирУгол;Интенс
0.0;0.0;;0.00;0.00;0.00;85.30;0.00;;0.00
100.0;2.5;45.0;1.54;1.54;99.90;-14.60;2.18;45.00;0.25
200.0;5.0;47.0;6.15;6.71;199.62;-114.32;9.10;47.48;0.25
```

---

## 3. LAS 2.0 (Log ASCII Standard)

### 3.1. Описание

Стандартный формат обмена каротажными данными. Incline3D поддерживает чтение и запись инклинометрических кривых.

### 3.2. Структура файла

```las
~VERSION INFORMATION
VERS.                          2.0 : CWLS LOG ASCII STANDARD - VERSION 2.0
WRAP.                          NO  : ONE LINE PER DEPTH STEP

~WELL INFORMATION
STRT.M                         0.0 : START DEPTH
STOP.M                      2500.0 : STOP DEPTH
STEP.M                         0.0 : STEP (0=IRREGULAR)
NULL.                      -999.25 : NULL VALUE
COMP.                  Компания    : COMPANY
WELL.                      123-1   : WELL
FLD.                   Приобское   : FIELD
LOC.                   Куст 123    : LOCATION
CTRY.                  Россия      : COUNTRY
SRVC.                              : SERVICE COMPANY
DATE.                  15-01-2024  : LOG DATE

~CURVE INFORMATION
DEPT.M                             : DEPTH
INCL.DEG                           : INCLINATION
AZIM.DEG                           : AZIMUTH (MAGNETIC)
AZIT.DEG                           : AZIMUTH (TRUE)

~ASCII LOG DATA
    0.0      0.00   -999.25   -999.25
  100.0      2.50     45.00     57.50
  200.0      5.00     47.00     59.50
  300.0      8.20     50.50     63.00
```

### 3.3. Поддерживаемые кривые

#### Входные кривые (импорт)

| Мнемоника | Альтернативы | Описание |
|-----------|--------------|----------|
| DEPT | MD, DEPTH | Глубина по стволу |
| INCL | INC, DEVI | Зенитный угол |
| AZIM | AZI, HAZI | Магнитный азимут |
| AZIT | TAZI, DAZI | Истинный/дирекционный азимут |

#### Выходные кривые (экспорт)

| Мнемоника | Единицы | Описание |
|-----------|---------|----------|
| DEPT | M | Глубина MD |
| INCL | DEG | Зенитный угол |
| AZIM | DEG | Магнитный азимут |
| AZIT | DEG | Истинный азимут |
| TVDSS | M | TVD от уровня моря |
| NORTH | M | Координата X (север) |
| EAST | M | Координата Y (восток) |
| DLS | DEG/30M | Dogleg severity |

### 3.4. Обработка NULL-значений

В LAS отсутствующие значения обозначаются специальным числом (обычно -999.25):

```cpp
constexpr double LAS_NULL_VALUE = -999.25;

bool isNullValue(double value, double null_value = LAS_NULL_VALUE) {
    return std::abs(value - null_value) < 1e-6;
}
```

### 3.5. Извлечение метаданных

Из секции `~WELL INFORMATION` извлекаются:

| Мнемоника | Поле IntervalData |
|-----------|-------------------|
| WELL | well |
| FLD | field |
| LOC | cluster + комментарий |
| SRVC | contractor |
| DATE | study_date |
| STRT | interval_start |
| STOP | interval_end |

### 3.6. Ограничения

1. **Поддерживается только LAS 2.0** — версия 3.0 планируется в будущем
2. **WRAP=NO** — данные на одной строке
3. **Единицы измерения** — метры и градусы
4. Метаданные скважины частично извлекаются автоматически

---

## 4. ZAK (Формат заключений)

### 4.1. Описание

Текстовый формат для обмена данными с системами формирования заключений. Используется для импорта результатов измерений.

### 4.2. Структура файла

```
#HEADER
VERSION=1.0
WELL=123-1
CLUSTER=123
FIELD=Приобское
DATE=15.01.2024
ALTITUDE=85.3
DECLINATION=12.5

#MEASUREMENTS
MD;INC;AZ
0.0;0.0;
100.0;2.5;45.0
200.0;5.0;47.0
300.0;8.2;50.5

#END
```

### 4.3. Секции файла

| Секция | Обязательность | Описание |
|--------|----------------|----------|
| #HEADER | Да | Метаданные скважины |
| #MEASUREMENTS | Да | Таблица замеров |
| #RESULTS | Нет | Результаты обработки (если есть) |
| #PROJECT_POINTS | Нет | Проектные точки |
| #END | Да | Маркер конца файла |

### 4.4. Поля заголовка

| Поле | Тип | Описание |
|------|-----|----------|
| VERSION | Строка | Версия формата |
| WELL | Строка | Номер скважины |
| CLUSTER | Строка | Номер куста |
| FIELD | Строка | Месторождение |
| DATE | Дата | Дата исследования (ДД.ММ.ГГГГ) |
| ALTITUDE | Число | Альтитуда стола ротора (м) |
| DECLINATION | Число | Магнитное склонение (°) |
| INTERVAL_START | Число | Начало интервала (м) |
| INTERVAL_END | Число | Конец интервала (м) |

### 4.5. Ограничения

1. **Только импорт** — экспорт в ZAK не поддерживается
2. Формат не полностью стандартизирован — возможны вариации
3. Кодировка: Windows-1251 или UTF-8 (автоопределение)

---

## 5. Шаблоны экспорта (*.mkl, *.mkz)

### 5.1. Концепция

Шаблоны позволяют настраивать формат вывода без изменения кода приложения. Шаблон описывает структуру выходного файла с подстановкой значений из результатов обработки.

### 5.2. Структура шаблона

```xml
<?xml version="1.0" encoding="UTF-8"?>
<export-template version="1.0">
    <metadata>
        <name>Стандартное заключение</name>
        <description>Экспорт в формате заключения по скважине</description>
        <extension>txt</extension>
        <encoding>UTF-8</encoding>
    </metadata>

    <header>
        <![CDATA[
ЗАКЛЮЧЕНИЕ ПО ИНКЛИНОМЕТРИИ

Скважина: ${well.name}
Куст: ${well.cluster}
Месторождение: ${well.field}
Дата: ${well.date}
Альтитуда: ${well.altitude} м
Магнитное склонение: ${well.declination}°

Метод расчёта: ${processing.method}
==============================================
        ]]>
    </header>

    <table>
        <columns>
            <column name="depth" header="Глубина" format="%.1f" width="10"/>
            <column name="inclination" header="Угол" format="%.2f" width="8"/>
            <column name="azimuth" header="Азимут" format="%.2f" width="8"/>
            <column name="x" header="X" format="%.2f" width="10"/>
            <column name="y" header="Y" format="%.2f" width="10"/>
            <column name="tvd" header="TVD" format="%.2f" width="10"/>
            <column name="intensity" header="Интенс" format="%.2f" width="8"/>
        </columns>
        <row-separator>\n</row-separator>
        <column-separator>\t</column-separator>
    </table>

    <footer>
        <![CDATA[
==============================================
Обработано: ${export.date}
        ]]>
    </footer>
</export-template>
```

### 5.3. Переменные подстановки

#### Метаданные скважины

| Переменная | Описание |
|------------|----------|
| `${well.name}` | Название/номер скважины |
| `${well.cluster}` | Номер куста |
| `${well.field}` | Месторождение |
| `${well.region}` | Регион |
| `${well.date}` | Дата исследования |
| `${well.altitude}` | Альтитуда стола ротора |
| `${well.declination}` | Магнитное склонение |
| `${well.current_bottom}` | Текущий забой |
| `${well.target_bottom}` | Проектный забой |

#### Результаты обработки

| Переменная | Описание |
|------------|----------|
| `${result.max_inclination}` | Макс. зенитный угол |
| `${result.max_inclination_depth}` | Глубина макс. угла |
| `${result.max_intensity}` | Макс. интенсивность |
| `${result.actual_shift}` | Фактическое смещение забоя |
| `${result.actual_azimuth}` | Фактический азимут забоя |

#### Параметры обработки

| Переменная | Описание |
|------------|----------|
| `${processing.method}` | Метод расчёта траектории |
| `${processing.azimuth_mode}` | Режим азимута |

#### Системные

| Переменная | Описание |
|------------|----------|
| `${export.date}` | Дата экспорта |
| `${export.time}` | Время экспорта |
| `${app.version}` | Версия приложения |

### 5.4. Колонки таблицы

| Имя колонки | Описание |
|-------------|----------|
| `depth` | Глубина MD |
| `inclination` | Зенитный угол |
| `azimuth` | Магнитный азимут |
| `true_azimuth` | Истинный азимут |
| `x` | Координата X |
| `y` | Координата Y |
| `tvd` | Вертикальная глубина |
| `absg` | Абсолютная отметка |
| `shift` | Смещение |
| `direction` | Дирекционный угол |
| `elongation` | Удлинение |
| `intensity` | Интенсивность на 10м |
| `intensity_l` | Интенсивность на L м |
| `error_x` | Погрешность X |
| `error_y` | Погрешность Y |
| `error_z` | Погрешность Z |

### 5.5. Расположение шаблонов

```
~/.config/incline3d/templates/    # Пользовательские
/usr/share/incline3d/templates/   # Системные (Linux)
C:\ProgramData\Incline3D\templates\  # Системные (Windows)
```

### 5.6. API шаблонов

```cpp
class ExportTemplate {
public:
    static ExportTemplate load(const std::filesystem::path& path);
    static std::vector<ExportTemplate> listAvailable();

    std::string name() const;
    std::string description() const;
    std::string extension() const;

    std::string render(const WellResult& result) const;
    void renderToFile(const WellResult& result,
                      const std::filesystem::path& output) const;
};
```

---

## 6. Реестр форматов

### 6.1. Архитектура

```cpp
class FormatRegistry {
public:
    static FormatRegistry& instance();

    // Регистрация
    void registerReader(std::unique_ptr<IFormatReader> reader);
    void registerWriter(std::unique_ptr<IFormatWriter> writer);

    // Поиск
    IFormatReader* findReader(const std::filesystem::path& path) const;
    IFormatWriter* findWriter(const std::string& format_name) const;

    // Перечисление
    std::vector<std::string> supportedImportFormats() const;
    std::vector<std::string> supportedExportFormats() const;
    std::string importFileFilter() const;  // Для диалога открытия файла
};
```

### 6.2. Автоопределение формата

```cpp
enum class FormatDetectionResult {
    Detected,       // Формат определён
    Ambiguous,      // Несколько возможных форматов
    Unknown         // Формат не распознан
};

struct DetectionInfo {
    FormatDetectionResult result;
    std::string format_name;
    std::vector<std::string> alternatives;  // При Ambiguous
    double confidence;                       // 0.0 - 1.0
};

DetectionInfo detectFormat(const std::filesystem::path& path);
```

Алгоритм определения:
1. Проверка расширения файла
2. Чтение первых байтов для проверки сигнатуры
3. Попытка разбора каждым доступным reader'ом

---

## 7. Обработка ошибок

### 7.1. Типы ошибок

```cpp
enum class FormatErrorCode {
    FileNotFound,
    PermissionDenied,
    InvalidFormat,
    MissingRequiredField,
    InvalidValue,
    EncodingError,
    VersionNotSupported,
    CorruptedFile
};

class FormatError : public InclineError {
public:
    FormatErrorCode code() const;
    std::string filename() const;
    std::optional<size_t> line_number() const;  // Для текстовых форматов
    std::string details() const;
};
```

### 7.2. Примеры сообщений

| Код | Сообщение |
|-----|-----------|
| FileNotFound | Файл не найден: /path/to/file.csv |
| InvalidFormat | Неверный формат файла. Ожидался CSV. |
| MissingRequiredField | Отсутствует обязательная колонка "Глубина" |
| InvalidValue | Строка 25: некорректное значение глубины "abc" |
| EncodingError | Ошибка декодирования: файл не в кодировке UTF-8 |
| VersionNotSupported | Версия формата 2.0 не поддерживается |

---

## 8. Рекомендации по расширению

### 8.1. Добавление нового формата импорта

1. Создать класс, реализующий `IFormatReader`
2. Реализовать методы:
   - `canRead()` — проверка возможности чтения
   - `read()` — чтение данных
   - `formatName()` — название формата
   - `extensions()` — расширения файлов
3. Зарегистрировать в `FormatRegistry`

```cpp
class MyFormatReader : public IFormatReader {
public:
    bool canRead(const std::filesystem::path& path) const override {
        // Проверка расширения и/или содержимого
    }

    IntervalData read(const std::filesystem::path& path) const override {
        // Парсинг файла
    }

    std::string_view formatName() const override {
        return "MyFormat";
    }

    std::vector<std::string> extensions() const override {
        return {".myf", ".myformat"};
    }
};

// Регистрация
FormatRegistry::instance().registerReader(
    std::make_unique<MyFormatReader>()
);
```

### 8.2. Добавление нового формата экспорта

Аналогично, с реализацией `IFormatWriter`.

### 8.3. Словари и справочники

Для поддержки различных вариаций названий полей используются словари:

```cpp
// Файл: field_aliases.json
{
    "depth": ["Глубина", "Depth", "MD", "DEPT", "ГЛ"],
    "inclination": ["Угол", "Inclination", "Inc", "INCL", "Зенит"],
    "magnetic_azimuth": ["Азимут", "Azimuth", "Az", "AZIM"]
}
```

Словари загружаются из конфигурационных файлов и могут быть дополнены пользователем.

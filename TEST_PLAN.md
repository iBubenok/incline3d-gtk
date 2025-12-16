# План тестирования Incline3D

## Обзор

Документ описывает стратегию тестирования приложения Incline3D: типы тестов, тестовые случаи, критерии приёмки и процедуры регрессионного тестирования.

## Структура тестирования

```
tests/
├── unit/                    # Модульные тесты
│   ├── test_angle_utils.cpp
│   ├── test_trajectory.cpp
│   ├── test_intensity.cpp
│   ├── test_errors.cpp
│   ├── test_interpolation.cpp
│   └── test_validation.cpp
├── integration/             # Интеграционные тесты
│   ├── test_csv_parser.cpp
│   ├── test_las_parser.cpp
│   ├── test_project_io.cpp
│   └── test_processing.cpp
├── regression/              # Регрессионные тесты
│   ├── test_reference_wells.cpp
│   └── reference_data/
│       ├── well_001.csv
│       ├── well_001_expected.json
│       └── ...
├── fixtures/                # Тестовые данные
│   ├── valid_csv/
│   ├── invalid_csv/
│   ├── las_files/
│   └── project_files/
└── test_main.cpp            # Точка входа для тестов
```

## Тестовый фреймворк

Используется **doctest** (подключается через CMake FetchContent):

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("normalizeAngle normalizes angles to [0, 360)") {
    CHECK(normalizeAngle(Degrees{-10.0}).value == doctest::Approx(350.0));
    CHECK(normalizeAngle(Degrees{370.0}).value == doctest::Approx(10.0));
    CHECK(normalizeAngle(Degrees{0.0}).value == doctest::Approx(0.0));
    CHECK(normalizeAngle(Degrees{360.0}).value == doctest::Approx(0.0));
}
```

---

## 1. Модульные тесты

### 1.1. Тесты нормализации и усреднения углов

**Файл:** `tests/unit/test_angle_utils.cpp`

#### Нормализация угла

| Тест | Вход | Ожидаемый выход |
|------|------|-----------------|
| Положительный угол в диапазоне | 45.0° | 45.0° |
| Угол = 0 | 0.0° | 0.0° |
| Угол = 360 | 360.0° | 0.0° |
| Угол > 360 | 370.0° | 10.0° |
| Угол > 720 | 730.0° | 10.0° |
| Отрицательный угол | -10.0° | 350.0° |
| Сильно отрицательный | -370.0° | 350.0° |
| NaN | NaN | NaN |

#### Усреднение азимутов

| Тест | Вход (a1, a2) | Ожидаемый выход |
|------|---------------|-----------------|
| Обычный случай | 10°, 20° | 15° |
| Через 0°/360° | 350°, 10° | 0° |
| Через 0°/360° (другой) | 355°, 5° | 0° |
| Равные углы | 45°, 45° | 45° |
| Противоположные (180°) | 0°, 180° | 90° или 270° (короткая дуга) |
| Один отсутствует | 45°, nullopt | nullopt |
| Оба отсутствуют | nullopt, nullopt | nullopt |

#### Интерполяция азимутов

| Тест | a1, d1 | a2, d2 | target_d | Ожидаемый |
|------|--------|--------|----------|-----------|
| Простой | 10°, 100м | 20°, 200м | 150м | 15° |
| Через 0°/360° | 350°, 100м | 10°, 200м | 150м | 0° |
| Экстраполяция вниз | 10°, 100м | 20°, 200м | 250м | 25° |

### 1.2. Тесты расчёта траектории

**Файл:** `tests/unit/test_trajectory.cpp`

#### Average Angle

| Тест | Inc1, Az1, Inc2, Az2, L | Ожидаемые ΔX, ΔY, ΔZ |
|------|-------------------------|----------------------|
| Вертикальный | 0°, -, 0°, -, 100м | 0, 0, 100 |
| Наклонный на восток | 45°, 90°, 45°, 90°, 141.42м | 0, 100, 100 |
| Наклонный на север | 45°, 0°, 45°, 0°, 141.42м | 100, 0, 100 |

#### Balanced Tangential

| Тест | Описание | Критерий |
|------|----------|----------|
| Вертикальный участок | Inc1=Inc2=0° | ΔX=ΔY=0, ΔZ=L |
| Прямолинейный наклонный | Inc1=Inc2, Az1=Az2 | Равен Average Angle |
| S-образный участок | Inc меняет знак | ΔZ < L |

#### Minimum Curvature

| Тест | Описание | Критерий |
|------|----------|----------|
| RF = 1 при DL = 0 | Прямолинейный | RF ≈ 1.0 |
| RF > 1 при DL > 0 | Криволинейный | RF > 1.0 |
| Малый DL | DL < 0.01° | RF ≈ 1.0 (без деления на 0) |

#### Ring Arc

| Тест | Описание | Критерий |
|------|----------|----------|
| Оба конца вертикальны | Inc1=Inc2=0° | ΔX=ΔY=0, ΔZ=L |
| Прямолинейный | Inc1=Inc2, Az1=Az2 | Совпадает с Balanced |
| Криволинейный | Inc1≠Inc2, Az1≠Az2 | Результат отличается от BT |

### 1.3. Тесты расчёта интенсивности

**Файл:** `tests/unit/test_intensity.cpp`

#### Dogleg angle

| Тест | Inc1, Az1, Inc2, Az2 | Ожидаемый DL |
|------|----------------------|--------------|
| Нулевой | 10°, 45°, 10°, 45° | 0° |
| Только зенит | 10°, 45°, 15°, 45° | 5° |
| Только азимут (экватор) | 90°, 0°, 90°, 10° | 10° |
| Комбинированный | 10°, 0°, 15°, 10° | ~6.4° |

#### Интенсивность на 10м

| Тест | DL, L | Ожидаемая I_10m |
|------|-------|-----------------|
| Стандартный | 2.5°, 100м | 0.25°/10м |
| Короткий интервал | 2.5°, 10м | 2.5°/10м |
| Нулевой DL | 0°, 100м | 0°/10м |
| Нулевая длина | 2.5°, 0м | 0 (защита от деления на 0) |

### 1.4. Тесты расчёта погрешностей

**Файл:** `tests/unit/test_errors.cpp`

| Тест | Описание | Критерий |
|------|----------|----------|
| Нулевой интервал | L = 0 | Погрешность = 0 |
| Вертикальный участок | Inc = 0° | σX ≈ 0, σY ≈ 0 |
| Накопление | Несколько интервалов | Монотонный рост |
| 95% множитель | Любой случай | Результат × 1.96 |

### 1.5. Тесты интерполяции

**Файл:** `tests/unit/test_interpolation.cpp`

| Тест | Описание | Критерий |
|------|----------|----------|
| Точное попадание | target = depth[i] | Без интерполяции |
| Между точками | depth[i] < target < depth[i+1] | Линейная интерполяция |
| До первой точки | target < depth[0] | Экстраполяция |
| После последней | target > depth[n-1] | Экстраполяция |
| Азимут через 0°/360° | Az1=350°, Az2=10° | Корректная интерполяция |

### 1.6. Тесты валидации

**Файл:** `tests/unit/test_validation.cpp`

| Тест | Вход | Ожидаемый результат |
|------|------|---------------------|
| Корректные данные | Валидный IntervalData | is_valid = true |
| Глубина вне диапазона | depth = -2000м | DepthOutOfRange |
| Угол > 180° | inclination = 200° | InclinationOutOfRange |
| Немонотонная глубина | d[i] < d[i-1] | NonMonotonicDepth |
| Пустой массив замеров | measurements.empty() | MissingRequiredField |

---

## 2. Интеграционные тесты

### 2.1. Тесты CSV-парсера

**Файл:** `tests/integration/test_csv_parser.cpp`

#### Успешный парсинг

| Тест | Файл | Критерий |
|------|------|----------|
| Стандартный CSV | `fixtures/valid_csv/standard.csv` | Все поля загружены |
| Запятая-разделитель | `fixtures/valid_csv/comma.csv` | Корректный разбор |
| Tab-разделитель | `fixtures/valid_csv/tab.csv` | Корректный разбор |
| С заголовком | `fixtures/valid_csv/with_header.csv` | Заголовок пропущен |
| Без заголовка | `fixtures/valid_csv/no_header.csv` | Всё данные |
| Windows-1251 | `fixtures/valid_csv/win1251.csv` | Корректная кодировка |
| Пустые ячейки | `fixtures/valid_csv/empty_cells.csv` | nullopt для азимутов |

#### Обработка ошибок

| Тест | Файл | Ожидаемая ошибка |
|------|------|------------------|
| Пустой файл | `fixtures/invalid_csv/empty.csv` | InvalidFormat |
| Некорректное число | `fixtures/invalid_csv/bad_number.csv` | InvalidValue |
| Отсутствует колонка | `fixtures/invalid_csv/missing_col.csv` | MissingRequiredField |

### 2.2. Тесты LAS-парсера

**Файл:** `tests/integration/test_las_parser.cpp`

| Тест | Файл | Критерий |
|------|------|----------|
| Стандартный LAS 2.0 | `fixtures/las_files/standard.las` | Все кривые загружены |
| С метаданными | `fixtures/las_files/with_meta.las` | well, field извлечены |
| NULL-значения | `fixtures/las_files/with_nulls.las` | nullopt для азимутов |
| Неверная версия | `fixtures/las_files/las3.las` | VersionNotSupported |

### 2.3. Тесты проектного файла

**Файл:** `tests/integration/test_project_io.cpp`

| Тест | Описание | Критерий |
|------|----------|----------|
| Сохранение/загрузка | Roundtrip | Данные идентичны |
| Миграция версий | v0.9 → v1.0 | Успешная миграция |
| Атомарность записи | Прерывание записи | Старый файл сохранён |
| Поврежденный JSON | Битый файл | CorruptedFile |

### 2.4. Тесты полной обработки

**Файл:** `tests/integration/test_processing.cpp`

| Тест | Описание | Критерий |
|------|----------|----------|
| Импорт → Обработка | CSV → WellResult | Результаты корректны |
| Все методы | AA, BT, MC, RA | Все завершаются успешно |
| Проектные точки | С Project_Points | Fact_* рассчитаны |
| Большой файл | 10000 точек | Завершается < 1с |

---

## 3. Регрессионные тесты

### 3.1. Эталонные скважины

**Файл:** `tests/regression/test_reference_wells.cpp`

Сравнение результатов с эталонными данными из проверенной системы.

#### Процедура получения эталона

1. Загрузить исходные данные в эталонную систему (Delphi)
2. Выполнить обработку всеми методами
3. Экспортировать результаты в JSON:
   ```json
   {
     "source": "well_001.csv",
     "method": "MinimumCurvature",
     "azimuth_mode": "Auto",
     "points": [
       {"depth": 0, "x": 0.0, "y": 0.0, "tvd": 0.0, "intensity": 0.0},
       {"depth": 100, "x": 1.54, "y": 1.54, "tvd": 99.90, "intensity": 0.25}
     ],
     "max_inclination": 45.0,
     "max_intensity": 2.5
   }
   ```
4. Сохранить как `well_001_expected.json`

#### Критерии соответствия

| Параметр | Допустимое отклонение |
|----------|----------------------|
| X, Y, TVD | ±0.01 м |
| ABSG | ±0.01 м |
| Смещение | ±0.01 м |
| Интенсивность | ±0.01 °/10м |
| Азимут | ±0.01° |
| Погрешности | ±0.1 м |

#### Тестовые скважины

| ID | Описание | Особенности |
|----|----------|-------------|
| well_001 | Простая наклонная | Базовый случай |
| well_002 | Вертикальная | Inc = 0° |
| well_003 | S-образная | Изменение направления |
| well_004 | Горизонтальная | Inc ≈ 90° |
| well_005 | С вертикальными участками | Inc < U_кр в середине |
| well_006 | Переход 0°/360° | Азимуты около севера |
| well_007 | Пропущенные азимуты | nullopt в середине |
| well_008 | Малые интервалы | ΔMD < 1м |
| well_009 | Большие интервалы | ΔMD > 100м |
| well_010 | Две точки | Минимальный случай |

### 3.2. Тестирование методов

Для каждого метода (AA, BT, MC, RA) проверяются все эталонные скважины.

```cpp
TEST_CASE("Minimum Curvature matches reference") {
    auto wells = loadReferenceWells();
    for (const auto& [source, expected] : wells) {
        auto result = processWell(source, TrajectoryMethod::MinimumCurvature);
        checkResults(result, expected, "MC");
    }
}
```

---

## 4. Граничные случаи

### 4.1. Особые значения

| Случай | Тест | Ожидание |
|--------|------|----------|
| Одна точка | 1 измерение | X=Y=0, TVD=0 |
| Две точки | 2 измерения | Один интервал |
| Нулевой интервал | depth[i] = depth[i-1] | ΔX=ΔY=ΔZ=0 |
| Очень малый интервал | ΔMD = 0.001м | Без деления на 0 |
| Inc = 0° | Вертикаль | X=Y=0 |
| Inc = 90° | Горизонталь | ΔZ ≈ 0 |
| Inc = 180° | Вверх | ΔZ < 0 |
| Az = 0° | Север | ΔY = 0 |
| Az = 90° | Восток | ΔX = 0 |
| Az = 180° | Юг | ΔX < 0 |
| Az = 270° | Запад | ΔY < 0 |

### 4.2. Числовая стабильность

| Случай | Тест | Ожидание |
|--------|------|----------|
| DL ≈ 0 | Прямолинейный участок | RF = 1.0, без NaN |
| sin(0) | Inc = 0° | Без деления на 0 |
| cos(0) | Inc = 90° | Корректный расчёт |
| acos(>1) | Ошибка округления | clamp(-1, 1) |
| Большие числа | depth = 15000м | Без переполнения |

---

## 5. Тесты производительности

### 5.1. Бенчмарки

| Операция | Размер | Целевое время |
|----------|--------|---------------|
| Обработка скважины | 100 точек | < 10 мс |
| Обработка скважины | 1000 точек | < 100 мс |
| Обработка скважины | 10000 точек | < 1 с |
| Импорт CSV | 10000 строк | < 500 мс |
| Импорт LAS | 10000 строк | < 500 мс |
| Сохранение проекта | 10 скважин | < 200 мс |
| Загрузка проекта | 10 скважин | < 200 мс |

### 5.2. Тесты памяти

| Сценарий | Критерий |
|----------|----------|
| Обработка большой скважины | Память освобождается после |
| Множественный импорт/удаление | Нет утечек (проверка Valgrind) |
| Длительная работа | Стабильное потребление памяти |

---

## 6. Запуск тестов

### 6.1. Команды

```bash
# Сборка тестов
cmake -B build -DBUILD_TESTING=ON
cmake --build build

# Запуск всех тестов
ctest --test-dir build --output-on-failure

# Запуск конкретной группы
ctest --test-dir build -R unit

# Запуск с подробным выводом
./build/tests/incline3d_tests -s

# Запуск конкретного теста
./build/tests/incline3d_tests -tc="normalizeAngle*"
```

### 6.2. Структура CMake

```cmake
# tests/CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

add_executable(incline3d_tests
    test_main.cpp
    unit/test_angle_utils.cpp
    unit/test_trajectory.cpp
    unit/test_intensity.cpp
    unit/test_errors.cpp
    unit/test_interpolation.cpp
    unit/test_validation.cpp
    integration/test_csv_parser.cpp
    integration/test_las_parser.cpp
    integration/test_project_io.cpp
    integration/test_processing.cpp
    regression/test_reference_wells.cpp
)

target_link_libraries(incline3d_tests PRIVATE
    incline3d_lib
    doctest::doctest
)

# Копирование тестовых данных
file(COPY fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Регистрация в CTest
include(doctest)
doctest_discover_tests(incline3d_tests)
```

---

## 7. Непрерывная интеграция

### 7.1. Матрица платформ

| Платформа | Компилятор | Конфигурация |
|-----------|------------|--------------|
| Ubuntu 22.04 | GCC 11 | Debug, Release |
| Ubuntu 22.04 | Clang 14 | Debug |
| Windows | MSVC 2022 | Debug, Release |
| Windows | MinGW-w64 | Release |

### 7.2. Этапы CI

```yaml
stages:
  - build
  - test
  - analyze

build:
  script:
    - cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTING=ON
    - cmake --build build --parallel

test:
  script:
    - ctest --test-dir build --output-on-failure
  artifacts:
    reports:
      junit: build/test_results.xml

analyze:
  script:
    - cppcheck --enable=all --error-exitcode=1 src/
    - clang-tidy src/**/*.cpp
```

---

## 8. Критерии приёмки

### 8.1. Покрытие кода

| Модуль | Минимальное покрытие |
|--------|---------------------|
| core/ | 90% |
| model/ | 85% |
| io/ | 80% |
| utils/ | 75% |
| Общее | 80% |

### 8.2. Качество тестов

- Все тесты проходят
- Нет flaky (нестабильных) тестов
- Время выполнения всех тестов < 60 секунд
- Отсутствие утечек памяти (Valgrind clean)

### 8.3. Регрессии

- Все эталонные скважины проходят
- Отклонения в пределах допуска
- Нет новых предупреждений компилятора

---

## 9. Шаблон тестового случая

```cpp
TEST_CASE("calculateTrajectory: Minimum Curvature method") {
    SUBCASE("vertical well returns zero horizontal displacement") {
        std::vector<MeasurementPoint> points = {
            {Meters{0}, Degrees{0}, std::nullopt, std::nullopt},
            {Meters{100}, Degrees{0}, std::nullopt, std::nullopt},
            {Meters{200}, Degrees{0}, std::nullopt, std::nullopt}
        };

        auto result = calculateTrajectory(points, TrajectoryMethod::MinimumCurvature);

        REQUIRE(result.size() == 3);
        CHECK(result[2].x.value == doctest::Approx(0.0).epsilon(0.01));
        CHECK(result[2].y.value == doctest::Approx(0.0).epsilon(0.01));
        CHECK(result[2].tvd.value == doctest::Approx(200.0).epsilon(0.01));
    }

    SUBCASE("inclined well on east bearing") {
        std::vector<MeasurementPoint> points = {
            {Meters{0}, Degrees{45}, Degrees{90}, std::nullopt},
            {Meters{141.42}, Degrees{45}, Degrees{90}, std::nullopt}
        };

        auto result = calculateTrajectory(points, TrajectoryMethod::MinimumCurvature);

        REQUIRE(result.size() == 2);
        CHECK(result[1].x.value == doctest::Approx(0.0).epsilon(0.1));
        CHECK(result[1].y.value == doctest::Approx(100.0).epsilon(0.1));
        CHECK(result[1].tvd.value == doctest::Approx(100.0).epsilon(0.1));
    }
}
```

---

## 10. Процедура добавления нового теста

1. Определить категорию теста (unit/integration/regression)
2. Создать файл или добавить в существующий
3. Написать тест с использованием doctest
4. Добавить в CMakeLists.txt если новый файл
5. Запустить локально и убедиться в прохождении
6. Добавить в CI если требуются специальные условия
7. Обновить документацию если новая функциональность

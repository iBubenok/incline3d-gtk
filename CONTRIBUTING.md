# Участие в разработке Incline3D

## Добро пожаловать

Благодарим за интерес к проекту Incline3D! Этот документ описывает правила и рекомендации для участников разработки.

## Содержание

1. [Требования к окружению](#требования-к-окружению)
2. [Сборка проекта](#сборка-проекта)
3. [Стиль кода](#стиль-кода)
4. [Процесс внесения изменений](#процесс-внесения-изменений)
5. [Тестирование](#тестирование)
6. [Документация](#документация)
7. [Важные правила](#важные-правила)

---

## Требования к окружению

### Обязательные компоненты

| Компонент | Минимальная версия |
|-----------|-------------------|
| CMake | 3.20 |
| Компилятор C++ | C++20 (GCC 11+, Clang 14+, MSVC 2022) |
| GTK | 4.x |

### Рекомендуемые инструменты

- **clang-format** — автоформатирование кода
- **clang-tidy** — статический анализ
- **cppcheck** — дополнительный статический анализ
- **Valgrind** (Linux) — проверка утечек памяти

---

## Сборка проекта

### Первоначальная настройка

```bash
# Клонирование репозитория
git clone https://github.com/iBubenok/incline3d-gtk.git
cd incline3d-gtk

# Сборка Debug
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build

# Запуск тестов
ctest --test-dir build --output-on-failure

# Запуск приложения
./build/incline3d
```

### Варианты сборки

```bash
# Release с оптимизациями
cmake -B build -DCMAKE_BUILD_TYPE=Release

# С Address Sanitizer (для отладки)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# Без тестов
cmake -B build -DBUILD_TESTING=OFF
```

---

## Стиль кода

### Общие принципы

1. **C++20** — используйте современные возможности языка
2. **RAII** — управление ресурсами через конструкторы/деструкторы
3. **Const-correctness** — всё, что может быть `const`, должно быть `const`
4. **Никаких «сырых» указателей для владения** — используйте `std::unique_ptr`, `std::shared_ptr`
5. **Предпочитайте `static_cast`** — не используйте C-style casts

### Именование

| Элемент | Стиль | Пример |
|---------|-------|--------|
| Файлы | `snake_case` | `angle_utils.cpp` |
| Пространства имён | `snake_case` | `incline::core` |
| Классы/структуры | `PascalCase` | `IntervalData` |
| Функции/методы | `camelCase` | `calculateTrajectory()` |
| Переменные | `snake_case` | `well_result` |
| Константы | `kPascalCase` | `kMaxDepth` |
| Члены класса | `snake_case_` | `measurements_` |
| Макросы | `UPPER_SNAKE_CASE` | `INCLINE_ASSERT` |

### Форматирование

Используется clang-format с конфигурацией `.clang-format`:

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Inline
PointerAlignment: Left
```

Для автоматического форматирования:

```bash
# Форматирование файла
clang-format -i src/core/trajectory.cpp

# Форматирование всех файлов
find src tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

### Пример кода

```cpp
#pragma once

#include <optional>
#include <vector>

namespace incline::core {

/// Результат расчёта приращения координат.
struct TrajectoryIncrement {
    Meters dx;
    Meters dy;
    Meters dz;
};

/// Рассчитывает приращение координат методом минимальной кривизны.
///
/// @param p1 Начальная точка интервала
/// @param p2 Конечная точка интервала
/// @return Приращения координат (dX, dY, dZ)
[[nodiscard]] TrajectoryIncrement calculateMinimumCurvature(
    const MeasurementPoint& p1,
    const MeasurementPoint& p2
);

class TrajectoryCalculator {
public:
    explicit TrajectoryCalculator(TrajectoryMethod method);

    [[nodiscard]] std::vector<ProcessedPoint> calculate(
        const std::vector<MeasurementPoint>& measurements,
        const ProcessingOptions& options
    ) const;

private:
    TrajectoryMethod method_;

    [[nodiscard]] TrajectoryIncrement calculateIncrement(
        const MeasurementPoint& p1,
        const MeasurementPoint& p2
    ) const;
};

} // namespace incline::core
```

### Комментарии

- **Doxygen** для публичного API (классы, функции)
- **Русский язык** для комментариев в коде
- Не комментируйте очевидное
- Комментируйте «почему», а не «что»

```cpp
// Плохо:
// Увеличиваем i на 1
++i;

// Хорошо:
// Пропускаем первую точку, т.к. она — устье с нулевыми координатами
for (size_t i = 1; i < points.size(); ++i) {
```

---

## Процесс внесения изменений

### 1. Создание ветки

```bash
git checkout -b feature/trajectory-ring-arc
# или
git checkout -b fix/azimuth-normalization
```

Именование веток:
- `feature/название` — новая функциональность
- `fix/название` — исправление ошибки
- `refactor/название` — рефакторинг
- `docs/название` — документация

### 2. Разработка

- Делайте атомарные коммиты (один коммит — одно логическое изменение)
- Пишите понятные сообщения коммитов
- Добавляйте тесты для нового кода
- Проверяйте, что существующие тесты проходят

### 3. Сообщения коммитов

Формат:
```
Категория: Краткое описание (до 50 символов)

Подробное описание (опционально).
Объясните что и почему, а не как.

Refs: #123 (ссылка на issue, если есть)
```

Категории:
- `feat` — новая функциональность
- `fix` — исправление ошибки
- `refactor` — рефакторинг без изменения поведения
- `test` — добавление/изменение тестов
- `docs` — документация
- `style` — форматирование, без изменения логики
- `build` — изменения в системе сборки

Примеры:
```
feat: Добавить метод Ring Arc для расчёта траектории

fix: Исправить нормализацию азимута около 360°

refactor: Выделить общую логику интерполяции в базовый класс
```

### 4. Pull Request

1. Убедитесь, что все тесты проходят
2. Убедитесь, что код отформатирован
3. Создайте Pull Request с описанием:
   - Что изменено
   - Зачем это нужно
   - Как тестировалось
4. Дождитесь review

---

## Тестирование

### Обязательные проверки перед коммитом

```bash
# Сборка
cmake --build build

# Тесты
ctest --test-dir build --output-on-failure

# Форматирование (проверка)
clang-format --dry-run --Werror src/**/*.cpp

# Статический анализ (опционально)
clang-tidy src/**/*.cpp
```

### Написание тестов

- Каждая новая функция должна иметь тесты
- Покрывайте граничные случаи
- Используйте doctest
- Размещайте тесты в соответствующих файлах

Пример:
```cpp
TEST_CASE("averageAzimuth handles 0/360 wrap-around") {
    CHECK(averageAzimuth(Degrees{350}, Degrees{10}).value().value
          == doctest::Approx(0.0).epsilon(0.01));
}
```

---

## Документация

### Обновление документации

При изменении функциональности обновите соответствующие документы:
- `README.md` — общее описание
- `ARCHITECTURE.md` — архитектурные изменения
- `DATA_MODEL.md` — изменения в структурах данных
- `ALGORITHMS.md` — изменения в алгоритмах
- `UI_SPEC.md` — изменения в интерфейсе
- `FILE_FORMATS.md` — изменения в форматах файлов
- `TEST_PLAN.md` — новые тесты

### Язык документации

- Публичная документация — **русский язык**
- Комментарии в коде — русский язык
- Сообщения коммитов — русский или английский

---

## Важные правила

### Строго запрещено

1. **Упоминание ИИ/AI в публичных файлах**
   - Нельзя упоминать Claude, ChatGPT, Copilot и т.п.
   - Нельзя упоминать «сгенерировано автоматически»

2. **Копирование кода из Delphi**
   - Delphi-код используется только как справочник
   - Все алгоритмы реализуются с нуля на C++

3. **Выполнение git-команд без согласования**
   - Не делайте `git push --force`
   - Не изменяйте историю опубликованных коммитов

4. **Изменение системных файлов**
   - Не устанавливайте пакеты глобально
   - Используйте локальные зависимости

### Рекомендации

1. **Маленькие PR лучше больших**
   - Легче review
   - Меньше конфликтов
   - Проще откатить

2. **Не оставляйте TODO без плана**
   - Каждый TODO должен содержать описание, что нужно сделать
   - Лучше создать issue, чем оставить TODO

3. **Тестируйте на разных платформах**
   - Если есть возможность, проверяйте на Linux и Windows

4. **Спрашивайте, если не уверены**
   - Создайте issue для обсуждения перед большими изменениями

---

## Контакты

**Yan Bubenok**
- Email: yan@bubenok.com
- Telegram: @iBubenok
- GitHub: [@iBubenok](https://github.com/iBubenok)

---

## Благодарности

Спасибо всем, кто вносит вклад в развитие проекта!

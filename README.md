# Incline3D

Кроссплатформенное приложение для обработки и визуализации данных инклинометрии скважин.

## Назначение

Incline3D предназначено для:

- **Ввода и редактирования** исходных данных замеров искривления ствола скважины
- **Обработки данных** — расчёт пространственной траектории (X, Y, TVD), интенсивностей искривления, погрешностей
- **Визуализации** одной или группы скважин:
  - 3D аксонометрия с интерактивным управлением
  - План (горизонтальная проекция)
  - Вертикальная проекция (разрез в произвольной плоскости)
- **Работы с проектными точками** — контроль попадания забоя в целевой пласт
- **Анализа**:
  - Сближения стволов скважин
  - Отхода фактической траектории от проектной
- **Экспорта и импорта** данных в различных форматах (CSV, LAS, ZAK)

## Поддерживаемые операционные системы

| ОС | Статус | Инструментарий |
|----|--------|---------------|
| **Linux** | ✅ Полная поддержка | GCC 11+, Clang 14+ |
| **Windows** | ✅ Полная поддержка | MSYS2/MinGW-w64, MSVC 2022 (core only) |
| **macOS** | ✅ Полная поддержка | Clang, Homebrew |

[![Build & Test](https://github.com/iBubenok/incline3d-gtk/actions/workflows/build.yml/badge.svg)](https://github.com/iBubenok/incline3d-gtk/actions/workflows/build.yml)

## Зависимости

### Обязательные

| Компонент | Минимальная версия | Назначение |
|-----------|-------------------|------------|
| CMake | 3.20 | Система сборки |
| Компилятор C++ | C++20 (GCC 11+, Clang 14+, MSVC 2022) | Сборка проекта |
| GTK | 4.x | Графический интерфейс (опционально) |
| OpenGL | 3.3+ | 3D-визуализация (опционально) |

> **Примечание:** GTK4 и OpenGL требуются только для GUI. Ядро (incline3d_core) можно собрать без них — для CLI, серверов или встраивания.

### Установка зависимостей

#### Linux (Debian/Ubuntu)

```bash
sudo apt install cmake build-essential ninja-build libgtk-4-dev libepoxy-dev
```

#### Linux (Fedora)

```bash
sudo dnf install cmake gcc-c++ ninja-build gtk4-devel libepoxy-devel
```

#### macOS (Homebrew)

```bash
brew install cmake ninja gtk4
```

#### Windows (MSYS2) — рекомендуемый способ

1. **Установите MSYS2** с https://www.msys2.org/

2. **Откройте терминал MINGW64** (не MSYS!)

3. **Установите зависимости:**
   ```bash
   pacman -Syu
   pacman -S mingw-w64-x86_64-cmake \
             mingw-w64-x86_64-ninja \
             mingw-w64-x86_64-gcc \
             mingw-w64-x86_64-gtk4 \
             mingw-w64-x86_64-libepoxy
   ```

4. **Соберите проект** (см. раздел "Сборка" ниже)

#### Windows (Visual Studio) — только ядро

Visual Studio 2022 может собрать только `incline3d_core` (без GUI):

```powershell
cmake -B build -DBUILD_GUI=OFF -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Принцип минимизации глобальных установок

Дополнительные зависимости (тестовые фреймворки, вспомогательные библиотеки) подключаются через CMake FetchContent и загружаются автоматически при первой сборке:

- **glm** — математика векторов/матриц
- **nlohmann_json** — работа с JSON
- **doctest** — модульные тесты

## Сборка

### Linux / macOS / MSYS2

```bash
# Debug-сборка (для разработки)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release-сборка (для продакшена)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Сборка только ядра (без GTK4)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF
cmake --build build
```

### Windows (PowerShell / CMD)

```powershell
# Только ядро (MSVC)
cmake -B build -DBUILD_GUI=OFF
cmake --build build --config Release

# Полная сборка (MSYS2 MINGW64)
# Выполнять в терминале MINGW64!
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Запуск тестов

```bash
# Linux / macOS / MSYS2
ctest --test-dir build --output-on-failure

# Windows (PowerShell)
ctest --test-dir build -C Release --output-on-failure
```

### Самопроверка рендеринга

Для быстрой smoke-проверки экспорта изображений:

```bash
./build/incline3d --render-selftest out
# Будут созданы out/axonometry.png, out/plan.png, out/vertical.png
```

### Диагностика и selftest

- Полный пакет диагностики (Markdown + JSON + изображения): `./build/incline3d --diagnostics --out out/diagnostics`
  - Артефакты: `report.md`, `report.json`, каталог `images/` с рендером.
  - Статусы: OK/WARN/FAIL/SKIPPED (SKIPPED для пунктов, недоступных без GUI).
- Демо-отчёт по анализам (сближение/отход, Markdown + CSV): `./build/incline3d --report-analyses --out out/analyses`

### Очистка

```bash
# Linux / macOS
rm -rf build

# Windows (PowerShell)
Remove-Item -Recurse -Force build
```

## Быстрый старт

### 1. Запуск приложения

```bash
./build/incline3d
```

### 2. Создание нового проекта

- Меню **Проект → Создать** или `Ctrl+N`
- Укажите имя проекта и путь для сохранения

### 3. Добавление скважины

**Вариант А: Импорт из файла**
- Меню **Исходные данные → Импорт**
- Выберите файл формата CSV, LAS или ZAK
- Настройте соответствие полей при необходимости

**Вариант Б: Ручной ввод**
- Меню **Исходные данные → Создать**
- Заполните метаданные скважины (месторождение, куст, номер)
- Введите замеры в таблицу: глубина, зенитный угол, азимут

### 4. Обработка данных

- Выберите скважину в списке
- Меню **Операции → Обработать** или `F5`
- Укажите параметры:
  - Метод расчёта траектории (усреднение углов / балансный / минимальная кривизна / кольцевые дуги)
  - Режим азимута (магнитный / истинный / авто)
- Нажмите **Выполнить**

### 5. Визуализация

- Переключайтесь между вкладками:
  - **Аксонометрия** — 3D-вид с вращением мышью
  - **План** — вид сверху
  - **Вертикальная проекция** — разрез в выбранной плоскости
- Управление 3D:
  - `ЛКМ + перемещение` — вращение
  - `Ctrl + ЛКМ` — панорамирование
  - `Shift + ЛКМ` или колесо — масштаб

### 6. Работа с проектными точками

- Выберите скважину
- В таблице **Проектные точки** добавьте целевые пласты
- Укажите: название, азимут, смещение, глубину/абсолютную отметку, радиус допуска
- На визуализации отобразятся круги допуска и отклонения

### 7. Экспорт результатов

**Изображения:**
- Меню **Проект → Сохранить изображение** или кнопка на панели
- Форматы: PNG, BMP
- Или копирование в буфер обмена

**Данные:**
- Меню **Результаты обработки → Экспорт**
- Выберите формат и поля для экспорта

### 8. Сохранение проекта

- Меню **Проект → Сохранить** или `Ctrl+S`
- Файл проекта `*.inclproj` содержит все скважины, настройки и результаты

## Структура проекта

```
incline3d-gtk/
├── CMakeLists.txt         # Конфигурация сборки
├── src/
│   ├── app/               # Точка входа, сборка приложения
│   ├── core/              # Алгоритмы расчёта траектории
│   ├── model/             # Структуры данных
│   ├── io/                # Чтение/запись форматов
│   ├── ui/                # GTK интерфейс
│   ├── rendering/         # 3D (OpenGL) и 2D (Cairo)
│   └── utils/             # Вспомогательные утилиты
├── tests/                 # Модульные и интеграционные тесты
├── resources/
│   ├── ui/                # XML-описания интерфейса
│   ├── icons/             # Иконки
│   └── shaders/           # GLSL шейдеры
└── docs/                  # Дополнительная документация
```

## Документация

- [ARCHITECTURE.md](ARCHITECTURE.md) — архитектура приложения
- [DATA_MODEL.md](DATA_MODEL.md) — модель данных
- [ALGORITHMS.md](ALGORITHMS.md) — алгоритмы расчёта
- [UI_SPEC.md](UI_SPEC.md) — спецификация интерфейса
- [FILE_FORMATS.md](FILE_FORMATS.md) — поддерживаемые форматы файлов
- [TEST_PLAN.md](TEST_PLAN.md) — план тестирования
- [CONTRIBUTING.md](CONTRIBUTING.md) — участие в разработке
- [DIAGNOSTICS.md](docs/DIAGNOSTICS.md) — пакет диагностики/selftest
- [ANALYSES.md](docs/ANALYSES.md) — анализы сближения/отхода
- [MANUAL_REGRESSION.md](docs/MANUAL_REGRESSION.md) — регрессия вручную

## Автор

**Yan Bubenok**

- Email: yan@bubenok.com
- Telegram: @iBubenok
- GitHub: [@iBubenok](https://github.com/iBubenok)

## Лицензия

Проект распространяется под лицензией MIT. Подробности в файле [LICENSE](LICENSE).

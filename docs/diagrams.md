# Диаграммы Incline3D

Документ содержит визуальные диаграммы архитектуры и потоков данных приложения.

## 1. Архитектурные слои

```mermaid
graph TB
    subgraph "App Layer"
        main[main.cpp]
        app[Application]
        session[Session]
    end

    subgraph "UI Layer"
        mw[MainWindow]
        dialogs[Dialogs]
        views[Views]
    end

    subgraph "Rendering Layer"
        axon[AxonometryView]
        plan[PlanView]
        vert[VerticalView]
        gl[OpenGL]
        cairo[Cairo]
    end

    subgraph "IO Layer"
        csv[CSV Reader/Writer]
        las[LAS Reader/Writer]
        proj[Project IO]
    end

    subgraph "Core Layer"
        traj[Trajectory]
        intens[Intensity]
        errors[Errors]
        interp[Interpolation]
    end

    subgraph "Model Layer"
        units[Units]
        data[Data Structures]
        valid[Validation]
    end

    subgraph "Utils Layer"
        log[Logging]
        file[File Utils]
        str[String Utils]
    end

    main --> app
    app --> session
    app --> mw

    mw --> dialogs
    mw --> views
    views --> axon
    views --> plan
    views --> vert

    axon --> gl
    plan --> cairo
    vert --> cairo

    session --> Core
    session --> IO

    Core --> Model
    IO --> Model
    Rendering --> Model

    Model --> Utils
    Core --> Utils
    IO --> Utils
```

## 2. Поток обработки данных

```mermaid
flowchart LR
    subgraph Input
        CSV[CSV файл]
        LAS[LAS файл]
        ZAK[ZAK файл]
        UI[Ручной ввод]
    end

    subgraph IO
        Reader[Format Reader]
    end

    subgraph Model
        ID[IntervalData]
    end

    subgraph Processing
        Validate[Валидация]
        Traj[Расчёт траектории]
        Intens[Расчёт интенсивности]
        Err[Расчёт погрешностей]
        PP[Расчёт проектных точек]
    end

    subgraph Results
        WR[WellResult]
        Points[ProcessedPoints]
    end

    subgraph Visualization
        D3[3D Аксонометрия]
        Plan[План]
        Vert[Верт. проекция]
    end

    subgraph Export
        ExpCSV[CSV]
        ExpLAS[LAS]
        ExpImg[Изображение]
    end

    CSV --> Reader
    LAS --> Reader
    ZAK --> Reader
    UI --> ID

    Reader --> ID
    ID --> Validate
    Validate --> Traj
    Traj --> Intens
    Intens --> Err
    Err --> PP
    PP --> WR

    WR --> Points
    Points --> D3
    Points --> Plan
    Points --> Vert

    WR --> ExpCSV
    WR --> ExpLAS
    D3 --> ExpImg
    Plan --> ExpImg
    Vert --> ExpImg
```

## 3. Структура данных скважины

```mermaid
classDiagram
    class IntervalData {
        +string uwi
        +string well
        +string cluster
        +string field
        +Meters interval_start
        +Meters interval_end
        +Degrees magnetic_declination
        +Meters rotor_table_altitude
        +vector~MeasurementPoint~ measurements
    }

    class MeasurementPoint {
        +Meters depth
        +Degrees inclination
        +OptionalAngle magnetic_azimuth
        +OptionalAngle true_azimuth
    }

    class WellResult {
        +string uwi
        +Meters actual_shift
        +Degrees max_inclination
        +double max_intensity
        +AzimuthMode azimuth_mode
        +TrajectoryMethod method
        +vector~ProcessedPoint~ points
        +vector~ProjectPoint~ project_points
    }

    class ProcessedPoint {
        +Meters depth
        +Degrees inclination
        +Meters x
        +Meters y
        +Meters tvd
        +Meters absg
        +double intensity_10m
        +Meters error_x
        +Meters error_y
    }

    class ProjectPoint {
        +string name
        +OptionalAngle azimuth_geographic
        +Meters shift
        +Meters depth
        +Meters radius
        +Factual factual
    }

    IntervalData "1" --> "*" MeasurementPoint
    WellResult "1" --> "*" ProcessedPoint
    WellResult "1" --> "*" ProjectPoint
```

## 4. Методы расчёта траектории

```mermaid
flowchart TB
    subgraph Input
        P1[Точка 1: MD₁, Inc₁, Az₁]
        P2[Точка 2: MD₂, Inc₂, Az₂]
    end

    subgraph Methods
        AA[Average Angle]
        BT[Balanced Tangential]
        MC[Minimum Curvature]
        RA[Ring Arc]
    end

    subgraph Calculation
        L[L = MD₂ - MD₁]
        DL[Dogleg Angle DL]
        RF[Ratio Factor RF]
    end

    subgraph Output
        dX[ΔX]
        dY[ΔY]
        dZ[ΔZ]
    end

    P1 --> L
    P2 --> L
    P1 --> DL
    P2 --> DL

    L --> AA
    L --> BT
    L --> MC
    L --> RA

    DL --> MC
    DL --> RA

    MC --> RF
    RA --> RF

    AA --> dX
    AA --> dY
    AA --> dZ

    BT --> dX
    BT --> dY
    BT --> dZ

    RF --> dX
    RF --> dY
    RF --> dZ
```

## 5. Цикл обработки скважины

```mermaid
sequenceDiagram
    participant User
    participant UI
    participant Session
    participant Core
    participant Model

    User->>UI: Импорт файла
    UI->>Session: loadFile(path)
    Session->>Core: parseFile(path)
    Core->>Model: IntervalData
    Model-->>Session: данные загружены
    Session-->>UI: обновить список

    User->>UI: Обработать
    UI->>Session: process(wellId, options)
    Session->>Core: calculateTrajectory()
    Core->>Core: для каждого интервала
    Core->>Core: calculateIncrement()
    Core->>Core: calculateIntensity()
    Core->>Core: calculateErrors()
    Core->>Model: WellResult
    Model-->>Session: результаты готовы
    Session-->>UI: обновить визуализацию
```

## 6. Структура главного окна

```mermaid
graph TB
    subgraph MainWindow
        Menu[Главное меню]
        Toolbar[Панель инструментов]

        subgraph Content
            subgraph LeftPanel[Панель данных]
                WellTable[Таблица скважин]
                ProjectPoints[Проектные точки]
                ShotPoints[Пункты возбуждения]
            end

            subgraph RightPanel[Область визуализации]
                Tabs[Вкладки]
                Axon[Аксонометрия]
                Plan[План]
                Vertical[Верт. проекция]
            end
        end

        StatusBar[Строка статуса]
    end

    Menu --> Content
    Toolbar --> Content
    Content --> StatusBar

    Tabs --> Axon
    Tabs --> Plan
    Tabs --> Vertical
```

## 7. Координатная система

```mermaid
graph LR
    subgraph Устье
        O[O: Устье скважины]
    end

    subgraph Оси
        X[+X: Север]
        Y[+Y: Восток]
        Z[+TVD: Вниз]
    end

    subgraph Углы
        Inc[Inc: Зенитный угол<br/>0° = вертикально]
        Az[Az: Азимут<br/>0° = Север]
    end

    O --> X
    O --> Y
    O --> Z
```

## 8. Обработка азимутов около 0°/360°

```mermaid
flowchart TB
    subgraph Вход
        A1[Az₁ = 350°]
        A2[Az₂ = 10°]
    end

    subgraph Расчёт
        D1[Прямая разность: 10° - 350° = -340°]
        D2[Через 360°: 350° + 360° - 10° = 20°]
        Check{20° < 340°?}
    end

    subgraph Результат
        Short[Короткая дуга через 0°]
        Avg[Среднее: 350° + 20°/2 = 360° = 0°]
    end

    A1 --> D1
    A2 --> D1
    A1 --> D2
    A2 --> D2
    D1 --> Check
    D2 --> Check
    Check -->|Да| Short
    Short --> Avg
```

## 9. Формат файла проекта

```mermaid
graph TB
    subgraph InclineProject[*.inclproj]
        Version[version: 1.0.0]

        subgraph Metadata
            Name[name]
            Description[description]
            Created[created]
        end

        subgraph Wells
            Well1[Well 1]
            Well2[Well 2]
            WellN[Well N]
        end

        subgraph Settings
            Global[global]
            Axon[axonometry]
            Plan[plan]
            Vertical[vertical]
        end
    end

    Version --> Metadata
    Metadata --> Wells
    Wells --> Settings

    Well1 --> SourceData[source_data]
    Well1 --> Result[result]
    Well1 --> ProjectPoints[project_points]
    Well1 --> ShotPoints[shot_points]
```

## 10. Жизненный цикл приложения

```mermaid
stateDiagram-v2
    [*] --> Запуск
    Запуск --> ПустойПроект: Новый проект
    Запуск --> ЗагрузкаПроекта: Открыть проект

    ЗагрузкаПроекта --> РаботаСДанными
    ПустойПроект --> РаботаСДанными: Импорт данных

    state РаботаСДанными {
        [*] --> Просмотр
        Просмотр --> Редактирование: Редактировать
        Редактирование --> Просмотр: Сохранить
        Просмотр --> Обработка: Обработать
        Обработка --> Просмотр: Завершено
        Просмотр --> Визуализация: Переключить вид
        Визуализация --> Просмотр: Вернуться
    }

    РаботаСДанными --> СохранениеПроекта: Сохранить
    СохранениеПроекта --> РаботаСДанными
    РаботаСДанными --> Экспорт: Экспорт
    Экспорт --> РаботаСДанными

    РаботаСДанными --> [*]: Выход
```

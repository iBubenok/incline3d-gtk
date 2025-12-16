# Алгоритмы расчёта Incline3D

## Обзор

Документ описывает математические алгоритмы, используемые для расчёта траектории скважины, интенсивности искривления, погрешностей и анализа данных.

## Содержание

1. [Нормализация и усреднение углов](#1-нормализация-и-усреднение-углов)
2. [Методы расчёта траектории](#2-методы-расчёта-траектории)
3. [Расчёт dogleg и интенсивности](#3-расчёт-dogleg-и-интенсивности)
4. [Обработка вертикальных участков](#4-обработка-вертикальных-участков)
5. [Расчёт погрешностей](#5-расчёт-погрешностей)
6. [Интерполяция для проектных точек](#6-интерполяция-для-проектных-точек)
7. [Анализ сближения и отхода](#7-анализ-сближения-и-отхода)

---

## 1. Нормализация и усреднение углов

### 1.1. Нормализация азимута

Приведение азимута к диапазону [0°, 360°):

```cpp
Degrees normalizeAngle(Degrees angle) {
    double a = angle.value;

    // Обработка специального значения "нет данных"
    if (std::isnan(a)) {
        return Degrees{std::nan("")};
    }

    // Приведение к положительному диапазону
    while (a < 0.0) {
        a += 360.0;
    }
    while (a >= 360.0) {
        a -= 360.0;
    }

    // Округление 360° до 0°
    if (std::abs(a - 360.0) < 1e-4) {
        a = 0.0;
    }

    return Degrees{a};
}
```

### 1.2. Усреднение азимутов

При усреднении азимутов необходимо учитывать переход через 0°/360°. Среднее вычисляется по **короткой дуге**:

```cpp
OptionalAngle averageAzimuth(OptionalAngle a1, OptionalAngle a2) {
    if (!a1.has_value() || !a2.has_value()) {
        return std::nullopt;
    }

    double v1 = normalizeAngle(a1.value()).value;
    double v2 = normalizeAngle(a2.value()).value;

    // Упорядочивание: v1 <= v2
    if (v1 > v2) {
        std::swap(v1, v2);
    }

    double diff_direct = v2 - v1;           // Прямая разность
    double diff_wrap = v1 + 360.0 - v2;     // Разность через 0°/360°

    double result;
    if (diff_wrap < diff_direct) {
        // Короткая дуга проходит через 0°/360°
        result = v2 + diff_wrap / 2.0;
    } else {
        // Короткая дуга не проходит через 0°/360°
        result = (v1 + v2) / 2.0;
    }

    return normalizeAngle(Degrees{result});
}
```

**Пример:**
- `average(350°, 10°)` → `0°` (не `180°`)
- `average(10°, 20°)` → `15°`

### 1.3. Интерполяция азимутов по глубине

При интерполяции азимута между двумя точками учитывается переход через 0°/360°:

```cpp
OptionalAngle interpolateAzimuth(
    Meters target_depth,
    OptionalAngle az1, Meters depth1,
    OptionalAngle az2, Meters depth2
) {
    if (!az1.has_value() || !az2.has_value()) {
        return std::nullopt;
    }

    double v1 = az1->value;
    double v2 = az2->value;
    double d1 = depth1.value;
    double d2 = depth2.value;
    double dt = target_depth.value;

    // Обработка перехода через 0°/360°
    if (v1 <= 180.0 && v2 > 180.0 && (v2 - v1) > 180.0) {
        v1 += 360.0;  // Трансформация в непрерывный диапазон
    } else if (v2 <= 180.0 && v1 > 180.0 && (v1 - v2) > 180.0) {
        v2 += 360.0;
    }

    // Линейная интерполяция
    double ratio = (dt - d1) / (d2 - d1);
    double result = v1 + ratio * (v2 - v1);

    return normalizeAngle(Degrees{result});
}
```

---

## 2. Методы расчёта траектории

### Общая схема расчёта

Для каждого интервала между точками `i-1` и `i` вычисляются приращения координат:
- `ΔX` — приращение на север
- `ΔY` — приращение на восток
- `ΔZ` — приращение вертикальной глубины (TVD)

Координаты накапливаются:
```
X[i] = X[i-1] + ΔX
Y[i] = Y[i-1] + ΔY
TVD[i] = TVD[i-1] + ΔZ
```

### Обозначения

| Символ | Описание |
|--------|----------|
| `L` | Длина интервала: `depth[i] - depth[i-1]` |
| `θ₁`, `θ₂` | Зенитные углы в начале и конце интервала (радианы) |
| `φ₁`, `φ₂` | Азимуты в начале и конце интервала (радианы) |
| `Δθ` | Разность зенитных углов: `θ₂ - θ₁` |
| `Δφ` | Разность азимутов (с учётом перехода 0/360) |

### 2.1. Average Angle (Усреднение углов)

Простейший метод. Используются средние значения углов на интервале.

```cpp
TrajectoryIncrement averageAngle(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) {
    double L = depth2.value - depth1.value;

    // Средние углы (в радианах)
    double theta_avg = toRadians(averageAngle(inc1, inc2)).value;
    auto az_avg = averageAzimuth(az1, az2);
    double phi_avg = az_avg ? toRadians(az_avg.value()).value : 0.0;

    double dX = L * std::sin(theta_avg) * std::cos(phi_avg);
    double dY = L * std::sin(theta_avg) * std::sin(phi_avg);
    double dZ = L * std::cos(theta_avg);

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}
```

**Точность:** низкая на криволинейных участках.

### 2.2. Balanced Tangential (Балансный тангенциальный)

Усредняются не углы, а компоненты направляющих векторов в начале и конце интервала.

```cpp
TrajectoryIncrement balancedTangential(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) {
    double L = depth2.value - depth1.value;

    double theta1 = toRadians(inc1).value;
    double theta2 = toRadians(inc2).value;
    double phi1 = az1 ? toRadians(az1.value()).value : 0.0;
    double phi2 = az2 ? toRadians(az2.value()).value : 0.0;

    // Компоненты направляющих векторов
    double dX = (L / 2.0) * (
        std::sin(theta1) * std::cos(phi1) +
        std::sin(theta2) * std::cos(phi2)
    );

    double dY = (L / 2.0) * (
        std::sin(theta1) * std::sin(phi1) +
        std::sin(theta2) * std::sin(phi2)
    );

    double dZ = (L / 2.0) * (std::cos(theta1) + std::cos(theta2));

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}
```

**Точность:** выше, чем Average Angle, особенно на немонотонных участках.

### 2.3. Minimum Curvature (Минимальная кривизна)

Наиболее распространённый метод. Траектория аппроксимируется дугой минимальной кривизны.

```cpp
TrajectoryIncrement minimumCurvature(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) {
    double L = depth2.value - depth1.value;

    double theta1 = toRadians(inc1).value;
    double theta2 = toRadians(inc2).value;
    double phi1 = az1 ? toRadians(az1.value()).value : 0.0;
    double phi2 = az2 ? toRadians(az2.value()).value : 0.0;

    // Расчёт dogleg angle (угла искривления)
    double cos_DL = std::cos(theta2 - theta1) -
                    std::sin(theta1) * std::sin(theta2) * (1.0 - std::cos(phi2 - phi1));
    cos_DL = std::clamp(cos_DL, -1.0, 1.0);
    double DL = std::acos(cos_DL);

    // Ratio Factor (RF)
    double RF;
    if (std::abs(DL) < 1e-7) {
        RF = 1.0;  // Прямолинейный участок
    } else {
        RF = (2.0 / DL) * std::tan(DL / 2.0);
    }

    // Приращения координат
    double dX = (L / 2.0) * RF * (
        std::sin(theta1) * std::cos(phi1) +
        std::sin(theta2) * std::cos(phi2)
    );

    double dY = (L / 2.0) * RF * (
        std::sin(theta1) * std::sin(phi1) +
        std::sin(theta2) * std::sin(phi2)
    );

    double dZ = (L / 2.0) * RF * (std::cos(theta1) + std::cos(theta2));

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}
```

**Точность:** высокая. Рекомендуется для большинства применений.

### 2.4. Ring Arc (Кольцевые дуги)

Метод сферической интерполяции. Траектория аппроксимируется дугой на сфере единичного радиуса.

```cpp
TrajectoryIncrement ringArc(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) {
    double L = depth2.value - depth1.value;

    double theta1 = toRadians(inc1).value;
    double theta2 = toRadians(inc2).value;
    double phi1 = az1 ? toRadians(az1.value()).value : 0.0;
    double phi2 = az2 ? toRadians(az2.value()).value : 0.0;

    // Особый случай: оба конца вертикальны
    if (std::abs(theta1) < 1e-7 && std::abs(theta2) < 1e-7) {
        return {Meters{0.0}, Meters{0.0}, Meters{L}};
    }

    // Особый случай: прямолинейный участок
    if (std::abs(theta1 - theta2) < 1e-7 && std::abs(phi1 - phi2) < 1e-7) {
        double dX = L * std::sin(theta1) * std::cos(phi1);
        double dY = L * std::sin(theta1) * std::sin(phi1);
        double dZ = L * std::cos(theta1);
        return {Meters{dX}, Meters{dY}, Meters{dZ}};
    }

    // Общий случай: сферическая дуга
    // Угол дуги на единичной сфере
    double cos_D = std::sin(theta1) * std::sin(theta2) * std::cos(phi1 - phi2) +
                   std::cos(theta1) * std::cos(theta2);
    cos_D = std::clamp(cos_D, -1.0, 1.0);
    double D = std::acos(cos_D);

    // Коэффициент масштабирования
    double factor;
    if (std::abs(D) < 1e-7) {
        factor = 1.0;
    } else {
        factor = std::tan(D / 2.0) / D;
    }

    double dX = L * factor * (
        std::sin(theta1) * std::cos(phi1) +
        std::sin(theta2) * std::cos(phi2)
    );

    double dY = L * factor * (
        std::sin(theta1) * std::sin(phi1) +
        std::sin(theta2) * std::sin(phi2)
    );

    double dZ = L * factor * (std::cos(theta1) + std::cos(theta2));

    return {Meters{dX}, Meters{dY}, Meters{dZ}};
}
```

> **Допущение:** Формула Ring Arc реализована на основе анализа эталонного кода. Требуется сверка результатов с эталоном на тестовых данных.
>
> **Как проверить:** Подготовить набор тестовых скважин с известными результатами из эталонной системы. Сравнить координаты X, Y, TVD с точностью до 4 знаков после запятой.

---

## 3. Расчёт dogleg и интенсивности

### 3.1. Dogleg Severity (пространственная интенсивность)

Dogleg — угол между направлениями ствола скважины в двух точках.

#### Метод через косинус (рекомендуемый)

```cpp
Radians calculateDogleg(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2
) {
    // Если азимут отсутствует — только зенитная компонента
    if (!az1.has_value() || !az2.has_value()) {
        return Radians{std::abs(toRadians(inc2).value - toRadians(inc1).value)};
    }

    double theta1 = toRadians(inc1).value;
    double theta2 = toRadians(inc2).value;
    double phi1 = toRadians(az1.value()).value;
    double phi2 = toRadians(az2.value()).value;

    double cos_DL = std::cos(theta2 - theta1) -
                    std::sin(theta1) * std::sin(theta2) * (1.0 - std::cos(phi2 - phi1));

    cos_DL = std::clamp(cos_DL, -1.0, 1.0);
    return Radians{std::acos(cos_DL)};
}
```

#### Альтернативный метод через синус

```cpp
Radians calculateDoglegSin(
    Degrees inc1, OptionalAngle az1,
    Degrees inc2, OptionalAngle az2
) {
    if (!az1.has_value() || !az2.has_value()) {
        return Radians{std::abs(toRadians(inc2).value - toRadians(inc1).value)};
    }

    double theta1 = toRadians(inc1).value;
    double theta2 = toRadians(inc2).value;
    double phi1 = toRadians(az1.value()).value;
    double phi2 = toRadians(az2.value()).value;

    double half_theta_diff = (theta2 - theta1) / 2.0;
    double half_theta_sum = (theta2 + theta1) / 2.0;
    double half_phi_diff = (phi2 - phi1) / 2.0;

    double sin_half_DL = std::sqrt(
        std::pow(std::sin(half_theta_diff), 2) +
        std::pow(std::sin(half_theta_sum) * std::sin(half_phi_diff), 2)
    );

    sin_half_DL = std::clamp(sin_half_DL, -1.0, 1.0);
    return Radians{2.0 * std::asin(sin_half_DL)};
}
```

### 3.2. Интенсивность на 10 метров

```cpp
double calculateIntensity10m(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2
) {
    double L = depth2.value - depth1.value;
    if (L < 1e-6) {
        return 0.0;
    }

    Radians DL = calculateDogleg(inc1, az1, inc2, az2);
    double DL_deg = toDegrees(DL).value;

    return (DL_deg * 10.0) / L;  // °/10м
}
```

### 3.3. Интенсивность на интервал L

```cpp
double calculateIntensityL(
    Meters depth1, Degrees inc1, OptionalAngle az1,
    Meters depth2, Degrees inc2, OptionalAngle az2,
    Meters interval_L
) {
    double L = depth2.value - depth1.value;
    if (L < 1e-6) {
        return 0.0;
    }

    Radians DL = calculateDogleg(inc1, az1, inc2, az2);
    double DL_deg = toDegrees(DL).value;

    return (DL_deg * interval_L.value) / L;  // °/L м
}
```

### 3.4. Зенитная интенсивность

Интенсивность только по зенитному углу (без учёта азимута):

```cpp
double calculateZenithIntensity10m(
    Meters depth1, Degrees inc1,
    Meters depth2, Degrees inc2
) {
    double L = depth2.value - depth1.value;
    if (L < 1e-6) {
        return 0.0;
    }

    double delta_inc = std::abs(inc2.value - inc1.value);
    return (delta_inc * 10.0) / L;
}
```

### 3.5. Сглаживание интенсивности

Для уменьшения шума интенсивность сглаживается в скользящем окне:

```cpp
std::vector<double> smoothIntensity(
    const std::vector<ProcessedPoint>& points,
    Meters window_half_size  // ±5м для интенсивности на 10м
) {
    std::vector<double> smoothed(points.size());

    for (size_t i = 0; i < points.size(); ++i) {
        double depth_i = points[i].depth.value;
        double sum = 0.0;
        int count = 0;

        for (size_t j = 0; j < points.size(); ++j) {
            double depth_j = points[j].depth.value;
            if (std::abs(depth_j - depth_i) <= window_half_size.value) {
                sum += points[j].intensity_10m;
                ++count;
            }
        }

        smoothed[i] = (count > 0) ? (sum / count) : 0.0;
    }

    return smoothed;
}
```

---

## 4. Обработка вертикальных участков

### 4.1. Критический угол U_кр

Параметр `critical_inclination` (U_кр) определяет порог вертикальности. Типичные значения: 0.25° — 1.0°.

```cpp
struct VerticalityConfig {
    Degrees critical_inclination{0.5};  // U_кр
    Meters near_surface_depth{30.0};     // Граница приустьевой зоны
};
```

### 4.2. Определение приустьевой зоны

Приустьевая зона — начальный участок скважины, где вертикальные точки не влияют на горизонтальное смещение:

```cpp
bool isNearSurface(
    Meters depth,
    const IntervalData& data,
    const VerticalityConfig& config
) {
    // Если задан башмак кондуктора — использовать его
    if (data.conductor_shoe.value > 0.0) {
        return depth.value <= data.conductor_shoe.value;
    }

    // Иначе — фиксированная глубина
    return depth.value <= config.near_surface_depth.value;
}
```

### 4.3. Алгоритм обработки вертикальных участков

```cpp
struct TrajectoryState {
    Meters x{0.0};
    Meters y{0.0};
    Meters tvd{0.0};
    bool had_non_vertical = false;  // Была ли уже невертикальная точка
};

void processPoint(
    TrajectoryState& state,
    const MeasurementPoint& prev,
    const MeasurementPoint& curr,
    const IntervalData& data,
    const VerticalityConfig& config,
    TrajectoryMethod method
) {
    bool is_vertical = curr.inclination.value <= config.critical_inclination.value;
    bool near_surface = isNearSurface(curr.depth, data, config);

    // Расчёт приращений
    auto [dX, dY, dZ] = calculateIncrement(prev, curr, method);

    // Вертикальная глубина всегда накапливается
    state.tvd = Meters{state.tvd.value + dZ.value};

    if (is_vertical) {
        if (near_surface) {
            // Приустьевая зона: X = Y = 0
            // Ничего не добавляем
        } else {
            // Глубже: X, Y фиксируются на предыдущем значении
            // Ничего не добавляем
        }
    } else {
        // Невертикальный участок: накапливаем смещение
        state.x = Meters{state.x.value + dX.value};
        state.y = Meters{state.y.value + dY.value};
        state.had_non_vertical = true;
    }
}
```

### 4.4. Обработка отсутствующего азимута

Если азимут отсутствует (`std::nullopt`), точка считается вертикальной независимо от зенитного угла:

```cpp
bool isEffectivelyVertical(
    const MeasurementPoint& point,
    const VerticalityConfig& config
) {
    // Нет азимута → вертикальная
    if (!point.magnetic_azimuth.has_value() && !point.true_azimuth.has_value()) {
        return true;
    }

    // Малый зенитный угол → вертикальная
    return point.inclination.value <= config.critical_inclination.value;
}
```

> **Допущение:** Граница приустьевой зоны определяется по башмаку кондуктора. Если не задан — используется 30 м.
>
> **Как проверить:** Сравнить результаты на скважинах с вертикальными участками на разных глубинах.

---

## 5. Расчёт погрешностей

### 5.1. Модель погрешностей

Погрешности рассчитываются по методу распространения ошибок (error propagation). Входные погрешности:
- `σ_D` — погрешность измерения глубины
- `σ_θ` — погрешность измерения зенитного угла
- `σ_φ` — погрешность измерения азимута

### 5.2. Формулы расчёта

Для каждого интервала вычисляются дисперсии приращений координат:

```cpp
struct ErrorContribution {
    double var_x;  // Дисперсия X
    double var_y;  // Дисперсия Y
    double var_z;  // Дисперсия Z
};

ErrorContribution calculateIntervalErrors(
    Meters depth1, Meters depth2,
    Degrees inc1, Degrees inc2,
    OptionalAngle az1, OptionalAngle az2,
    Degrees sigma_inc,    // σ_θ
    Degrees sigma_az,     // σ_φ
    Meters sigma_depth    // σ_D
) {
    double L = depth2.value - depth1.value;

    // Средние углы (в радианах)
    double theta_avg = toRadians(averageAngle(inc1, inc2)).value;
    auto az_avg_opt = averageAzimuth(az1, az2);
    double phi_avg = az_avg_opt ? toRadians(az_avg_opt.value()).value : 0.0;

    double sigma_theta = toRadians(sigma_inc).value;
    double sigma_phi = toRadians(sigma_az).value;
    double sigma_d = sigma_depth.value;

    // Частные производные для X
    double dX_dD = std::sin(theta_avg) * std::cos(phi_avg);
    double dX_dphi = -L * std::sin(theta_avg) * std::sin(phi_avg);
    double dX_dtheta = L * std::cos(theta_avg) * std::cos(phi_avg);

    // Дисперсия X
    double var_x = std::pow(dX_dD * sigma_d, 2) +
                   std::pow(dX_dphi * sigma_phi, 2) +
                   std::pow(dX_dtheta * sigma_theta, 2);
    var_x /= 2.0;  // Делитель 2 — эмпирический коэффициент

    // Частные производные для Y
    double dY_dD = std::sin(theta_avg) * std::sin(phi_avg);
    double dY_dphi = L * std::sin(theta_avg) * std::cos(phi_avg);
    double dY_dtheta = L * std::cos(theta_avg) * std::sin(phi_avg);

    // Дисперсия Y
    double var_y = std::pow(dY_dD * sigma_d, 2) +
                   std::pow(dY_dphi * sigma_phi, 2) +
                   std::pow(dY_dtheta * sigma_theta, 2);
    var_y /= 2.0;

    // Дисперсия Z
    double var_z;
    if (L > 1e-6) {
        var_z = std::pow(std::cos(theta_avg) * sigma_d, 2) +
                std::pow(L * std::sin(theta_avg) * sigma_theta, 2);
    } else {
        var_z = 0.0;
    }

    return {var_x, var_y, var_z};
}
```

### 5.3. Накопление погрешностей

Дисперсии накапливаются по мере прохождения по траектории:

```cpp
struct AccumulatedErrors {
    double var_x = 0.0;
    double var_y = 0.0;
    double var_z = 0.0;
};

void accumulateErrors(AccumulatedErrors& acc, const ErrorContribution& contrib) {
    acc.var_x += contrib.var_x;
    acc.var_y += contrib.var_y;
    acc.var_z += contrib.var_z;
}

// Получение погрешностей с 95% доверительным интервалом
struct Errors95 {
    Meters error_x;
    Meters error_y;
    Meters error_z;
};

Errors95 getErrors95(const AccumulatedErrors& acc) {
    constexpr double k95 = 1.96;  // Коэффициент для 95% доверительного интервала
    return {
        Meters{std::sqrt(acc.var_x) * k95},
        Meters{std::sqrt(acc.var_y) * k95},
        Meters{std::sqrt(acc.var_z) * k95}
    };
}
```

### 5.4. Погрешность интенсивности

```cpp
double calculateIntensityError(
    double intensity_10m,
    Degrees sigma_inc,
    Degrees sigma_az,
    Meters interval_length
) {
    // Упрощённая оценка
    double sigma_total = std::sqrt(
        std::pow(sigma_inc.value, 2) + std::pow(sigma_az.value, 2)
    );

    return (sigma_total * 10.0) / interval_length.value;
}
```

> **Допущение:** Формула погрешностей основана на анализе эталонного кода. Делитель 2 в формулах дисперсий — эмпирический.
>
> **Как проверить:** Сравнить расчётные погрешности с эталонными на тестовых скважинах.

---

## 6. Интерполяция для проектных точек

### 6.1. Интерполяция по глубине MD

Для получения параметров на заданной глубине (проектная точка):

```cpp
ProcessedPoint interpolateByDepth(
    Meters target_depth,
    const std::vector<ProcessedPoint>& points
) {
    // Поиск охватывающего интервала
    auto it = std::lower_bound(
        points.begin(), points.end(), target_depth,
        [](const ProcessedPoint& p, Meters d) { return p.depth.value < d.value; }
    );

    if (it == points.begin()) {
        return points.front();  // Экстраполяция вверх
    }
    if (it == points.end()) {
        return points.back();   // Экстраполяция вниз
    }

    const auto& p1 = *(it - 1);
    const auto& p2 = *it;

    double ratio = (target_depth.value - p1.depth.value) /
                   (p2.depth.value - p1.depth.value);

    ProcessedPoint result;
    result.depth = target_depth;

    // Линейная интерполяция числовых полей
    result.x = Meters{p1.x.value + ratio * (p2.x.value - p1.x.value)};
    result.y = Meters{p1.y.value + ratio * (p2.y.value - p1.y.value)};
    result.tvd = Meters{p1.tvd.value + ratio * (p2.tvd.value - p1.tvd.value)};
    result.absg = Meters{p1.absg.value + ratio * (p2.absg.value - p1.absg.value)};
    result.shift = Meters{p1.shift.value + ratio * (p2.shift.value - p1.shift.value)};

    result.inclination = Degrees{
        p1.inclination.value + ratio * (p2.inclination.value - p1.inclination.value)
    };

    // Азимуты — с учётом перехода 0/360
    result.magnetic_azimuth = interpolateAzimuth(
        target_depth,
        p1.magnetic_azimuth, p1.depth,
        p2.magnetic_azimuth, p2.depth
    );
    result.true_azimuth = interpolateAzimuth(
        target_depth,
        p1.true_azimuth, p1.depth,
        p2.true_azimuth, p2.depth
    );

    // Интенсивности
    result.intensity_10m = p1.intensity_10m + ratio * (p2.intensity_10m - p1.intensity_10m);
    result.intensity_L = p1.intensity_L + ratio * (p2.intensity_L - p1.intensity_L);

    return result;
}
```

### 6.2. Интерполяция по абсолютной отметке

Если проектная точка задана абсолютной отметкой:

```cpp
ProcessedPoint interpolateByAbsDepth(
    Meters target_abs,
    const std::vector<ProcessedPoint>& points
) {
    // Поиск по ABSG (от большего к меньшему, т.к. глубина растёт)
    auto it = std::lower_bound(
        points.begin(), points.end(), target_abs,
        [](const ProcessedPoint& p, Meters a) { return p.absg.value > a.value; }
    );

    // Далее аналогично interpolateByDepth
    // ...
}
```

### 6.3. Расчёт фактических параметров проектной точки

```cpp
void calculateFactual(
    ProjectPoint& pp,
    const std::vector<ProcessedPoint>& points,
    Meters rotor_table_altitude
) {
    Meters target_depth;

    if (pp.depth.has_value()) {
        target_depth = pp.depth.value();
    } else if (pp.abs_depth.has_value()) {
        // Преобразование абсолютной отметки в MD требует итерации
        target_depth = findDepthByAbsDepth(pp.abs_depth.value(), points);
    } else {
        return;  // Нет данных для расчёта
    }

    auto interpolated = interpolateByDepth(target_depth, points);

    pp.factual = ProjectPoint::Factual{
        .inclination = interpolated.inclination,
        .magnetic_azimuth = interpolated.magnetic_azimuth,
        .true_azimuth = interpolated.true_azimuth,
        .shift = interpolated.shift,
        .elongation = interpolated.elongation,
        .x = interpolated.x,
        .y = interpolated.y,
        .deviation = calculateDeviation(pp, interpolated),
        .deviation_direction = calculateDeviationDirection(pp, interpolated),
        .tvd = interpolated.tvd,
        .intensity_10m = interpolated.intensity_10m,
        .intensity_L = interpolated.intensity_L
    };
}
```

---

## 7. Анализ сближения и отхода

### 7.1. Расчёт отхода от проектной точки

```cpp
struct DeviationResult {
    Meters distance;           // Расстояние до проектной точки
    Degrees direction_angle;   // Дирекционный угол отхода
    bool within_tolerance;     // Попадание в круг допуска
};

DeviationResult calculateDeviation(
    const ProjectPoint& pp,
    const ProcessedPoint& actual
) {
    // Проектные координаты
    double proj_x, proj_y;
    if (pp.base_shift.has_value()) {
        // Относительно базовой точки
        double base_az = pp.base_azimuth.has_value() ?
            toRadians(pp.base_azimuth.value().value()).value : 0.0;
        proj_x = pp.base_shift.value().value * std::cos(base_az) +
                 pp.shift.value * std::cos(toRadians(pp.azimuth_geographic.value()).value);
        proj_y = pp.base_shift.value().value * std::sin(base_az) +
                 pp.shift.value * std::sin(toRadians(pp.azimuth_geographic.value()).value);
    } else {
        // Относительно устья
        double az = pp.azimuth_geographic.has_value() ?
            toRadians(pp.azimuth_geographic.value()).value : 0.0;
        proj_x = pp.shift.value * std::cos(az);
        proj_y = pp.shift.value * std::sin(az);
    }

    // Фактические координаты
    double fact_x = actual.x.value;
    double fact_y = actual.y.value;

    // Расстояние (отход)
    double dx = fact_x - proj_x;
    double dy = fact_y - proj_y;
    double distance = std::sqrt(dx*dx + dy*dy);

    // Дирекционный угол отхода
    double direction = std::atan2(dy, dx);
    if (direction < 0) {
        direction += 2.0 * M_PI;
    }

    return {
        Meters{distance},
        toDegrees(Radians{direction}),
        distance <= pp.radius.value
    };
}
```

### 7.2. Анализ сближения стволов

Расчёт минимального расстояния между двумя траекториями:

```cpp
struct ProximityResult {
    Meters min_distance;      // Минимальное расстояние
    Meters depth1;            // Глубина на первой скважине
    Meters depth2;            // Глубина на второй скважине
    Meters tvd;               // TVD точки сближения
};

ProximityResult analyzeProximity(
    const std::vector<ProcessedPoint>& well1,
    const std::vector<ProcessedPoint>& well2,
    Meters step = Meters{1.0}  // Шаг дискретизации
) {
    ProximityResult result;
    result.min_distance = Meters{std::numeric_limits<double>::max()};

    // Дискретизация первой траектории
    for (const auto& p1 : well1) {
        // Поиск ближайшей точки на второй траектории
        for (const auto& p2 : well2) {
            double dx = p1.x.value - p2.x.value;
            double dy = p1.y.value - p2.y.value;
            double dz = p1.tvd.value - p2.tvd.value;
            double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

            if (dist < result.min_distance.value) {
                result.min_distance = Meters{dist};
                result.depth1 = p1.depth;
                result.depth2 = p2.depth;
                result.tvd = Meters{(p1.tvd.value + p2.tvd.value) / 2.0};
            }
        }
    }

    return result;
}
```

### 7.3. Расчёт горизонтального сближения

Сближение только в горизонтальной плоскости (план):

```cpp
ProximityResult analyzeHorizontalProximity(
    const std::vector<ProcessedPoint>& well1,
    const std::vector<ProcessedPoint>& well2
) {
    ProximityResult result;
    result.min_distance = Meters{std::numeric_limits<double>::max()};

    for (const auto& p1 : well1) {
        for (const auto& p2 : well2) {
            // Проверяем только точки на близких TVD
            if (std::abs(p1.tvd.value - p2.tvd.value) > 10.0) {
                continue;
            }

            double dx = p1.x.value - p2.x.value;
            double dy = p1.y.value - p2.y.value;
            double dist = std::sqrt(dx*dx + dy*dy);

            if (dist < result.min_distance.value) {
                result.min_distance = Meters{dist};
                result.depth1 = p1.depth;
                result.depth2 = p2.depth;
                result.tvd = Meters{(p1.tvd.value + p2.tvd.value) / 2.0};
            }
        }
    }

    return result;
}
```

---

## Приложение А: Сводка формул

### Приращения координат

| Метод | ΔX | ΔY | ΔZ |
|-------|----|----|-----|
| Average Angle | `L·sin(θ_avg)·cos(φ_avg)` | `L·sin(θ_avg)·sin(φ_avg)` | `L·cos(θ_avg)` |
| Balanced Tangential | `L/2·(sin(θ₁)cos(φ₁)+sin(θ₂)cos(φ₂))` | `L/2·(sin(θ₁)sin(φ₁)+sin(θ₂)sin(φ₂))` | `L/2·(cos(θ₁)+cos(θ₂))` |
| Minimum Curvature | Balanced Tangential × RF | Balanced Tangential × RF | Balanced Tangential × RF |
| Ring Arc | Balanced Tangential × `tan(D/2)/D` | Balanced Tangential × `tan(D/2)/D` | Balanced Tangential × `tan(D/2)/D` |

### Ratio Factor (RF) для Minimum Curvature

```
RF = (2/DL)·tan(DL/2)   при DL ≠ 0
RF = 1                   при DL = 0
```

### Dogleg angle (DL)

```
cos(DL) = cos(θ₂-θ₁) - sin(θ₁)·sin(θ₂)·(1-cos(φ₂-φ₁))
```

### Интенсивность

```
I_10m = DL_deg × 10 / L    (°/10м)
I_L = DL_deg × L_int / L   (°/L м)
```

---

## Приложение Б: Контрольные точки для тестирования

### Тест 1: Вертикальная скважина

| MD (м) | Inc (°) | Az (°) | X (м) | Y (м) | TVD (м) |
|--------|---------|--------|-------|-------|---------|
| 0 | 0 | — | 0 | 0 | 0 |
| 100 | 0 | — | 0 | 0 | 100 |
| 200 | 0 | — | 0 | 0 | 200 |

### Тест 2: Наклонная прямая (45° на восток)

| MD (м) | Inc (°) | Az (°) | X (м) | Y (м) | TVD (м) |
|--------|---------|--------|-------|-------|---------|
| 0 | 45 | 90 | 0 | 0 | 0 |
| 141.42 | 45 | 90 | 0 | 100 | 100 |

### Тест 3: Переход через 0°/360°

| MD (м) | Inc (°) | Az_1 (°) | Az_2 (°) | Az_avg (°) |
|--------|---------|----------|----------|------------|
| — | — | 350 | 10 | 0 |
| — | — | 355 | 5 | 0 |
| — | — | 10 | 20 | 15 |

---

## Приложение В: Требования к тестированию алгоритмов

### Обязательные тесты

1. **Нормализация углов**
   - Отрицательные азимуты
   - Азимуты > 360°
   - Граничные случаи (0°, 360°, 180°)

2. **Усреднение азимутов**
   - Обычные случаи
   - Переход через 0°/360°
   - Отсутствующие азимуты

3. **Методы траектории**
   - Вертикальная скважина
   - Наклонная прямая
   - S-образная траектория
   - Горизонтальный участок

4. **Вертикальные участки**
   - Приустьевая зона
   - Вертикальный участок в середине
   - Переход вертикальный → наклонный

5. **Интенсивность**
   - Прямолинейный участок (I = 0)
   - Постоянная кривизна
   - Резкий излом

6. **Погрешности**
   - Накопление на длинной траектории
   - Влияние погрешностей измерений

### Регрессионные тесты

Сравнение результатов с эталонной системой:
- Подготовить 10+ реальных скважин с известными результатами
- Допустимое отклонение: X, Y, TVD — ±0.01 м; интенсивность — ±0.01°/10м

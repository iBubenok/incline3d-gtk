/**
 * @file test_trajectory.cpp
 * @brief Unit-тесты для методов расчёта траектории
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include <doctest/doctest.h>
#include "core/trajectory.hpp"
#include <cmath>

using namespace incline::core;
using namespace incline::model;

TEST_CASE("Вертикальный участок (зенит = 0)") {
    Meters d1{0.0}, d2{100.0};
    Degrees inc1{0.0}, inc2{0.0};
    OptionalAngle az1 = std::nullopt;
    OptionalAngle az2 = std::nullopt;

    SUBCASE("Average Angle") {
        auto result = averageAngle(d1, inc1, az1, d2, inc2, az2);
        CHECK(result.dx.value == doctest::Approx(0.0));
        CHECK(result.dy.value == doctest::Approx(0.0));
        CHECK(result.dz.value == doctest::Approx(100.0));
    }

    SUBCASE("Balanced Tangential") {
        auto result = balancedTangential(d1, inc1, az1, d2, inc2, az2);
        CHECK(result.dx.value == doctest::Approx(0.0));
        CHECK(result.dy.value == doctest::Approx(0.0));
        CHECK(result.dz.value == doctest::Approx(100.0));
    }

    SUBCASE("Minimum Curvature") {
        auto result = minimumCurvature(d1, inc1, az1, d2, inc2, az2);
        CHECK(result.dx.value == doctest::Approx(0.0));
        CHECK(result.dy.value == doctest::Approx(0.0));
        CHECK(result.dz.value == doctest::Approx(100.0));
    }

    SUBCASE("Ring Arc") {
        auto result = ringArc(d1, inc1, az1, d2, inc2, az2);
        CHECK(result.dx.value == doctest::Approx(0.0));
        CHECK(result.dy.value == doctest::Approx(0.0));
        CHECK(result.dz.value == doctest::Approx(100.0));
    }
}

TEST_CASE("Горизонтальный участок (зенит = 90°)") {
    Meters d1{0.0}, d2{100.0};
    Degrees inc1{90.0}, inc2{90.0};
    OptionalAngle az1 = Degrees{0.0};  // Север
    OptionalAngle az2 = Degrees{0.0};

    SUBCASE("Minimum Curvature") {
        auto result = minimumCurvature(d1, inc1, az1, d2, inc2, az2);
        // При зените 90° и азимуте 0° (Север), смещение по X
        CHECK(result.dx.value == doctest::Approx(100.0).epsilon(0.01));
        CHECK(result.dy.value == doctest::Approx(0.0).epsilon(0.01));
        CHECK(result.dz.value == doctest::Approx(0.0).epsilon(0.01));
    }

    SUBCASE("При азимуте 90° (Восток)") {
        az1 = Degrees{90.0};
        az2 = Degrees{90.0};

        auto result = minimumCurvature(d1, inc1, az1, d2, inc2, az2);
        CHECK(result.dx.value == doctest::Approx(0.0).epsilon(0.01));
        CHECK(result.dy.value == doctest::Approx(100.0).epsilon(0.01));
        CHECK(result.dz.value == doctest::Approx(0.0).epsilon(0.01));
    }
}

TEST_CASE("Наклонный участок") {
    Meters d1{0.0}, d2{100.0};
    Degrees inc1{45.0}, inc2{45.0};
    OptionalAngle az1 = Degrees{0.0};  // Север
    OptionalAngle az2 = Degrees{0.0};

    auto result = minimumCurvature(d1, inc1, az1, d2, inc2, az2);

    // При зените 45° смещение по X и Z должны быть примерно равны
    double expected = 100.0 * std::sin(45.0 * std::numbers::pi / 180.0);
    CHECK(result.dx.value == doctest::Approx(expected).epsilon(0.01));
    CHECK(result.dy.value == doctest::Approx(0.0).epsilon(0.01));
    CHECK(result.dz.value == doctest::Approx(expected).epsilon(0.01));
}

TEST_CASE("Сравнение методов на простом участке") {
    Meters d1{100.0}, d2{200.0};
    Degrees inc1{5.0}, inc2{10.0};
    OptionalAngle az1 = Degrees{45.0};
    OptionalAngle az2 = Degrees{50.0};

    auto aa = averageAngle(d1, inc1, az1, d2, inc2, az2);
    auto bt = balancedTangential(d1, inc1, az1, d2, inc2, az2);
    auto mc = minimumCurvature(d1, inc1, az1, d2, inc2, az2);

    // Все методы должны давать положительное смещение dz
    CHECK(aa.dz.value > 90.0);
    CHECK(bt.dz.value > 90.0);
    CHECK(mc.dz.value > 90.0);

    // Все методы должны давать примерно одинаковый результат на малых углах
    CHECK(aa.dz.value == doctest::Approx(bt.dz.value).epsilon(1.0));
    CHECK(bt.dz.value == doctest::Approx(mc.dz.value).epsilon(1.0));
}

TEST_CASE("calculateIncrement выбирает правильный метод") {
    Meters d1{0.0}, d2{100.0};
    Degrees inc1{10.0}, inc2{15.0};
    OptionalAngle az1 = Degrees{30.0};
    OptionalAngle az2 = Degrees{35.0};

    auto aa = calculateIncrement(d1, inc1, az1, d2, inc2, az2, TrajectoryMethod::AverageAngle);
    auto bt = calculateIncrement(d1, inc1, az1, d2, inc2, az2, TrajectoryMethod::BalancedTangential);
    auto mc = calculateIncrement(d1, inc1, az1, d2, inc2, az2, TrajectoryMethod::MinimumCurvature);
    auto ra = calculateIncrement(d1, inc1, az1, d2, inc2, az2, TrajectoryMethod::RingArc);

    // Проверяем что результаты разумные
    CHECK(aa.dz.value > 0);
    CHECK(bt.dz.value > 0);
    CHECK(mc.dz.value > 0);
    CHECK(ra.dz.value > 0);

    // Minimum Curvature и Ring Arc должны давать близкие результаты
    CHECK(mc.dx.value == doctest::Approx(ra.dx.value).epsilon(1.0));
    CHECK(mc.dy.value == doctest::Approx(ra.dy.value).epsilon(1.0));
    CHECK(mc.dz.value == doctest::Approx(ra.dz.value).epsilon(1.0));
}

TEST_CASE("Нулевой интервал") {
    Meters d1{100.0}, d2{100.0};
    Degrees inc1{30.0}, inc2{30.0};
    OptionalAngle az1 = Degrees{45.0};
    OptionalAngle az2 = Degrees{45.0};

    auto result = minimumCurvature(d1, inc1, az1, d2, inc2, az2);
    CHECK(result.dx.value == doctest::Approx(0.0));
    CHECK(result.dy.value == doctest::Approx(0.0));
    CHECK(result.dz.value == doctest::Approx(0.0));
}

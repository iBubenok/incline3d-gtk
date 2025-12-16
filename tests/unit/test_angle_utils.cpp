/**
 * @file test_angle_utils.cpp
 * @brief Unit-тесты для функций работы с углами
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include <doctest/doctest.h>
#include "core/angle_utils.hpp"
#include <cmath>

using namespace incline::core;
using namespace incline::model;

TEST_CASE("normalizeAngle") {
    SUBCASE("Углы в допустимом диапазоне") {
        CHECK(normalizeAngle(Degrees{0.0}).value == doctest::Approx(0.0));
        CHECK(normalizeAngle(Degrees{180.0}).value == doctest::Approx(180.0));
        CHECK(normalizeAngle(Degrees{359.9}).value == doctest::Approx(359.9));
    }

    SUBCASE("Углы >= 360") {
        CHECK(normalizeAngle(Degrees{360.0}).value == doctest::Approx(0.0));
        CHECK(normalizeAngle(Degrees{361.0}).value == doctest::Approx(1.0));
        CHECK(normalizeAngle(Degrees{720.0}).value == doctest::Approx(0.0));
        CHECK(normalizeAngle(Degrees{450.0}).value == doctest::Approx(90.0));
    }

    SUBCASE("Отрицательные углы") {
        CHECK(normalizeAngle(Degrees{-1.0}).value == doctest::Approx(359.0));
        CHECK(normalizeAngle(Degrees{-90.0}).value == doctest::Approx(270.0));
        CHECK(normalizeAngle(Degrees{-360.0}).value == doctest::Approx(0.0));
        CHECK(normalizeAngle(Degrees{-450.0}).value == doctest::Approx(270.0));
    }
}

TEST_CASE("averageInclination") {
    SUBCASE("Простое усреднение") {
        CHECK(averageInclination(Degrees{0.0}, Degrees{10.0}).value == doctest::Approx(5.0));
        CHECK(averageInclination(Degrees{30.0}, Degrees{40.0}).value == doctest::Approx(35.0));
    }

    SUBCASE("Одинаковые углы") {
        CHECK(averageInclination(Degrees{45.0}, Degrees{45.0}).value == doctest::Approx(45.0));
    }
}

TEST_CASE("averageAzimuth") {
    SUBCASE("Оба пустые") {
        auto result = averageAzimuth(std::nullopt, std::nullopt);
        CHECK_FALSE(result.has_value());
    }

    SUBCASE("Один пустой") {
        auto result1 = averageAzimuth(Degrees{45.0}, std::nullopt);
        REQUIRE(result1.has_value());
        CHECK(result1->value == doctest::Approx(45.0));

        auto result2 = averageAzimuth(std::nullopt, Degrees{90.0});
        REQUIRE(result2.has_value());
        CHECK(result2->value == doctest::Approx(90.0));
    }

    SUBCASE("Обычное усреднение") {
        auto result = averageAzimuth(Degrees{40.0}, Degrees{50.0});
        REQUIRE(result.has_value());
        CHECK(result->value == doctest::Approx(45.0));
    }

    SUBCASE("Переход через 0/360 градусов") {
        // 350° и 10° -> среднее должно быть около 0° (или 360°)
        auto result = averageAzimuth(Degrees{350.0}, Degrees{10.0});
        REQUIRE(result.has_value());
        // Среднее между 350 и 10 через 0 = 0 (или 360)
        CHECK((result->value == doctest::Approx(0.0).epsilon(0.1) ||
               result->value == doctest::Approx(360.0).epsilon(0.1)));
    }

    SUBCASE("Противоположные азимуты") {
        auto result = averageAzimuth(Degrees{0.0}, Degrees{180.0});
        REQUIRE(result.has_value());
        CHECK(result->value == doctest::Approx(90.0));
    }
}

TEST_CASE("azimuthDifference") {
    SUBCASE("Простая разница") {
        CHECK(azimuthDifference(Degrees{50.0}, Degrees{40.0}).value == doctest::Approx(10.0));
        CHECK(azimuthDifference(Degrees{40.0}, Degrees{50.0}).value == doctest::Approx(-10.0));
    }

    SUBCASE("Переход через 0/360") {
        // От 350 к 10 = +20
        CHECK(azimuthDifference(Degrees{10.0}, Degrees{350.0}).value == doctest::Approx(20.0));
        // От 10 к 350 = -20
        CHECK(azimuthDifference(Degrees{350.0}, Degrees{10.0}).value == doctest::Approx(-20.0));
    }

    SUBCASE("Противоположные направления") {
        // От 0 к 180 = +180 или -180
        auto diff = azimuthDifference(Degrees{180.0}, Degrees{0.0});
        CHECK(std::abs(diff.value) == doctest::Approx(180.0));
    }
}

TEST_CASE("Конвертация Degrees <-> Radians") {
    SUBCASE("Degrees to Radians") {
        CHECK(Degrees{0.0}.toRadians().value == doctest::Approx(0.0));
        CHECK(Degrees{90.0}.toRadians().value == doctest::Approx(std::numbers::pi / 2));
        CHECK(Degrees{180.0}.toRadians().value == doctest::Approx(std::numbers::pi));
        CHECK(Degrees{360.0}.toRadians().value == doctest::Approx(2 * std::numbers::pi));
    }

    SUBCASE("Radians to Degrees") {
        CHECK(Radians{0.0}.toDegrees().value == doctest::Approx(0.0));
        CHECK(Radians{std::numbers::pi / 2}.toDegrees().value == doctest::Approx(90.0));
        CHECK(Radians{std::numbers::pi}.toDegrees().value == doctest::Approx(180.0));
    }
}

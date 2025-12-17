/**
 * @file test_azimuth_processing.cpp
 * @brief Тесты обработки азимутов (интерполяция/blanking/extend-last)
 */

#include <doctest/doctest.h>
#include "core/processing.hpp"
#include <optional>

using namespace incline::core;
using namespace incline::model;

TEST_CASE("Интерполяция пропусков азимута по короткой дуге") {
    IntervalData data;
    MeasurementPoint m1;
    m1.depth = Meters{0.0};
    m1.inclination = Degrees{10.0};
    m1.magnetic_azimuth = Degrees{350.0};
    data.measurements.push_back(m1);

    MeasurementPoint m2;
    m2.depth = Meters{50.0};
    m2.inclination = Degrees{10.0};
    data.measurements.push_back(m2);

    MeasurementPoint m3;
    m3.depth = Meters{100.0};
    m3.inclination = Degrees{10.0};
    m3.magnetic_azimuth = Degrees{10.0};
    data.measurements.push_back(m3);

    ProcessingOptions options;
    options.interpolate_missing_azimuths = true;
    options.blank_vertical_azimuth = false;
    options.vertical_if_no_azimuth = true;

    auto result = processWell(data, options);
    REQUIRE(result.points.size() == 3);
    REQUIRE(result.points[1].computed_azimuth.has_value());
    CHECK(result.points[1].computed_azimuth->value == doctest::Approx(0.0).epsilon(0.01));
}

TEST_CASE("Продление последнего известного азимута") {
    IntervalData data;
    MeasurementPoint m1;
    m1.depth = Meters{0.0};
    m1.inclination = Degrees{15.0};
    m1.magnetic_azimuth = Degrees{45.0};
    data.measurements.push_back(m1);

    MeasurementPoint m2;
    m2.depth = Meters{50.0};
    m2.inclination = Degrees{15.0};
    data.measurements.push_back(m2);

    MeasurementPoint m3;
    m3.depth = Meters{100.0};
    m3.inclination = Degrees{15.0};
    data.measurements.push_back(m3);

    ProcessingOptions options;
    options.extend_last_azimuth = true;
    options.blank_vertical_azimuth = false;
    options.vertical_if_no_azimuth = true;

    auto result = processWell(data, options);
    REQUIRE(result.points.size() == 3);
    REQUIRE(result.points[1].computed_azimuth.has_value());
    REQUIRE(result.points[2].computed_azimuth.has_value());
    CHECK(result.points[1].computed_azimuth->value == doctest::Approx(45.0));
    CHECK(result.points[2].computed_azimuth->value == doctest::Approx(45.0));
}

TEST_CASE("Blanking азимута на вертикальном интервале") {
    IntervalData data;
    MeasurementPoint m1;
    m1.depth = Meters{0.0};
    m1.inclination = Degrees{0.0};
    m1.magnetic_azimuth = Degrees{90.0};
    data.measurements.push_back(m1);

    MeasurementPoint m2;
    m2.depth = Meters{50.0};
    m2.inclination = Degrees{0.0};
    m2.magnetic_azimuth = Degrees{90.0};
    data.measurements.push_back(m2);

    ProcessingOptions options;
    options.blank_vertical_azimuth = true;
    options.vertical_if_no_azimuth = true;

    auto result = processWell(data, options);
    REQUIRE(result.points.size() == 2);
    CHECK_FALSE(result.points[1].computed_azimuth.has_value());
    CHECK(result.points[1].intensity_10m == doctest::Approx(0.0));
    CHECK(result.points[1].x.value == doctest::Approx(0.0));
    CHECK(result.points[1].y.value == doctest::Approx(0.0));
}

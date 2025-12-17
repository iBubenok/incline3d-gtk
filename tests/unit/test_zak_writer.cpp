/**
 * @file test_zak_writer.cpp
 * @brief Тесты экспорта/импорта формата ZAK
 */

#include <doctest/doctest.h>
#include "io/zak_writer.hpp"
#include "io/zak_reader.hpp"
#include <filesystem>

using namespace incline::io;
using namespace incline::model;

namespace {

std::filesystem::path makeTempPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("ZAK writer: round-trip UTF-8") {
    IntervalData data;
    data.well = "123-1";
    data.cluster = "Куст-5";
    data.field = "Месторождение";
    data.study_date = "2025-12-17";
    data.rotor_table_altitude = Meters{150.5};
    data.ground_altitude = Meters{120.0};
    data.magnetic_declination = Degrees{8.5};
    data.interval_start = Meters{0.0};
    data.interval_end = Meters{300.0};
    data.contractor = "Подрядчик";

    MeasurementPoint p1;
    p1.depth = Meters{0.0};
    p1.inclination = Degrees{0.0};
    p1.magnetic_azimuth = Degrees{0.0};

    MeasurementPoint p2;
    p2.depth = Meters{100.0};
    p2.inclination = Degrees{12.3};
    p2.magnetic_azimuth = Degrees{45.0};
    p2.true_azimuth = Degrees{46.0};

    data.measurements.clear();
    data.measurements.push_back(p1);
    data.measurements.push_back(p2);

    auto path = makeTempPath("incline3d_zak_writer_utf8.zak");
    ZakWriteOptions options;
    options.decimal_places = 1;

    writeZak(data, path, options);

    auto parsed = readZak(path);
    std::filesystem::remove(path);

    CHECK(parsed.well == data.well);
    CHECK(parsed.cluster == data.cluster);
    CHECK(parsed.field == data.field);
    CHECK(parsed.contractor == data.contractor);
    CHECK(parsed.magnetic_declination.value == doctest::Approx(data.magnetic_declination.value));
    CHECK(parsed.rotor_table_altitude.value == doctest::Approx(data.rotor_table_altitude.value));
    CHECK(parsed.ground_altitude.value == doctest::Approx(data.ground_altitude.value));
    REQUIRE(parsed.measurements.size() == 2);
    CHECK(parsed.measurements[1].true_azimuth.has_value());
    CHECK(parsed.measurements[1].true_azimuth->value == doctest::Approx(46.0));
}

TEST_CASE("ZAK writer: CP1251 encoding detected on read") {
    IntervalData data;
    data.well = "Скв-1";
    data.cluster = "Куст-А";
    data.field = "Поле";

    MeasurementPoint p1;
    p1.depth = Meters{10.0};
    p1.inclination = Degrees{1.1};
    p1.magnetic_azimuth = Degrees{350.0};
    data.measurements.push_back(p1);

    auto path = makeTempPath("incline3d_zak_writer_cp1251.zak");
    ZakWriteOptions options;
    options.encoding = "CP1251";
    options.use_crlf = true;

    writeZak(data, path, options);

    auto parsed = readZak(path);
    std::filesystem::remove(path);

    CHECK(parsed.well == data.well);
    CHECK(parsed.cluster == data.cluster);
    CHECK(parsed.field == data.field);
    REQUIRE(parsed.measurements.size() == 1);
    CHECK(parsed.measurements[0].depth.value == doctest::Approx(10.0));
    CHECK(parsed.measurements[0].magnetic_azimuth.has_value());
    CHECK(parsed.measurements[0].magnetic_azimuth->value == doctest::Approx(350.0));
}

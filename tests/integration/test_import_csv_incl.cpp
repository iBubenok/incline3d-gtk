/**
 * @file test_import_csv_incl.cpp
 * @brief Интеграционный тест импорта CSV (Incl.csv)
 */

#include <doctest/doctest.h>
#include "io/csv_reader.hpp"
#include <filesystem>
#include <algorithm>

using namespace incline::io;
using namespace incline::model;

namespace {

std::filesystem::path fixturePath(const std::string& name) {
    return std::filesystem::path(INCLINE3D_SOURCE_DIR) / "tests" / "fixtures" / name;
}

bool isMonotonicNonDecreasing(const MeasurementList& measurements) {
    for (size_t i = 1; i < measurements.size(); ++i) {
        if (measurements[i].depth.value + 1e-9 < measurements[i - 1].depth.value) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("Incl.csv imports successfully with auto-detection") {
    auto path = fixturePath("Incl.csv");
    CsvReadOptions options;
    options.encoding = "AUTO";

    auto data = readCsvMeasurements(path, options);

    REQUIRE(!data.measurements.empty());
    CHECK(data.measurements.size() > 200);
    CHECK(isMonotonicNonDecreasing(data.measurements));

    for (const auto& m : data.measurements) {
        CHECK(m.depth.value >= 0.0);
        CHECK(m.inclination.value >= 0.0);
        CHECK(m.inclination.value <= 180.0);
    }
}

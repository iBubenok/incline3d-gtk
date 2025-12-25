/**
 * @file test_import_las_gir1.cpp
 * @brief Интеграционный тест импорта LAS (gir1.las)
 */

#include <doctest/doctest.h>
#include "io/las_reader.hpp"
#include <filesystem>

using namespace incline::io;
using namespace incline::model;

namespace {

std::filesystem::path fixturePath(const std::string& name) {
    return std::filesystem::path(INCLINE3D_SOURCE_DIR) / "tests" / "fixtures" / name;
}

bool isMonotonicDepth(const MeasurementList& measurements) {
    for (size_t i = 1; i < measurements.size(); ++i) {
        if (measurements[i].depth.value + 1e-9 < measurements[i - 1].depth.value) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("gir1.las imports with depth and zenith detected") {
    auto path = fixturePath("gir1.las");
    LasReadOptions options;
    auto data = readLasMeasurements(path, options);

    REQUIRE(!data.measurements.empty());
    CHECK(data.measurements.size() > 1000);
    CHECK(isMonotonicDepth(data.measurements));
    for (const auto& m : data.measurements) {
        CHECK(m.inclination.value >= 0.0);
        CHECK(m.inclination.value <= 180.0);
    }
}

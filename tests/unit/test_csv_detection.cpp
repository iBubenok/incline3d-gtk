/**
 * @file test_csv_detection.cpp
 * @brief Юнит-тесты автоопределения CSV
 */

#include <doctest/doctest.h>
#include "io/csv_reader.hpp"
#include <filesystem>
#include <fstream>

using namespace incline::io;

namespace {

std::filesystem::path makeSourcePath(const std::string& relative) {
    return std::filesystem::path(INCLINE3D_SOURCE_DIR) / relative;
}

} // namespace

TEST_CASE("CSV detection handles CP1251 headers and zenith aliases") {
    auto path = makeSourcePath("tests/fixtures/Incl.csv");
    auto detection = detectCsvFormat(path);

    CHECK(detection.detected_encoding == "CP1251");
    REQUIRE(detection.has_header);
    REQUIRE(detection.suggested_mapping.depth_column.has_value());
    REQUIRE(detection.suggested_mapping.inclination_column.has_value());
    CHECK(detection.detected_delimiter == ';');
    CHECK(*detection.suggested_mapping.depth_column == 0);
    CHECK(*detection.suggested_mapping.inclination_column == 1);
}

TEST_CASE("CSV detection falls back to numeric stats when headers are unknown") {
    auto path = std::filesystem::temp_directory_path() / "incline3d_csv_detect_numeric.csv";
    {
        std::ofstream ofs(path);
        ofs << "ColA;ColB;ColC\n";
        ofs << "0;1.1;350\n";
        ofs << "10;2.2;351\n";
        ofs << "20;3.3;352\n";
    }

    auto detection = detectCsvFormat(path);
    std::error_code ec;
    std::filesystem::remove(path, ec);

    REQUIRE(detection.suggested_mapping.depth_column.has_value());
    REQUIRE(detection.suggested_mapping.inclination_column.has_value());
    CHECK(*detection.suggested_mapping.depth_column == 0);
    CHECK(*detection.suggested_mapping.inclination_column == 1);
}

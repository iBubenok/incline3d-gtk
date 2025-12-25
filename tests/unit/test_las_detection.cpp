/**
 * @file test_las_detection.cpp
 * @brief Юнит-тесты автоопределения кривых LAS
 */

#include <doctest/doctest.h>
#include "io/las_reader.hpp"
#include <filesystem>

using namespace incline::io;

namespace {

std::filesystem::path fixturePath(const std::string& name) {
    return std::filesystem::path(INCLINE3D_SOURCE_DIR) / "tests" / "fixtures" / name;
}

} // namespace

TEST_CASE("LAS detection recognizes ZENIT as inclination") {
    auto curves = getLasCurves(fixturePath("gir1.las"));
    REQUIRE(!curves.empty());

    auto detection = detectLasCurves(curves, {});
    REQUIRE(detection.depth_index.has_value());
    REQUIRE(detection.inclination_index.has_value());

    CHECK(curves[static_cast<size_t>(*detection.depth_index)].mnemonic == "DEPTH");
    CHECK(curves[static_cast<size_t>(*detection.inclination_index)].mnemonic == "ZENIT");
}

/**
 * @file project_io.cpp
 * @brief Реализация работы с файлами проекта
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "project_io.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

namespace incline::io {

using json = nlohmann::json;

namespace {

// === Сериализация базовых типов ===

json metersToJson(Meters m) {
    return m.value;
}

Meters metersFromJson(const json& j) {
    return Meters{j.get<double>()};
}

json degreesToJson(Degrees d) {
    return d.value;
}

Degrees degreesFromJson(const json& j) {
    return Degrees{j.get<double>()};
}

json optionalAngleToJson(const OptionalAngle& angle) {
    if (angle.has_value()) {
        return angle->value;
    }
    return nullptr;
}

OptionalAngle optionalAngleFromJson(const json& j) {
    if (j.is_null()) {
        return std::nullopt;
    }
    return Degrees{j.get<double>()};
}

json colorToJson(const Color& c) {
    return c.toHex();
}

Color colorFromJson(const json& j) {
    return Color::fromHex(j.get<std::string>());
}

json projectPointFactualToJson(const ProjectPointFactual& f) {
    json j;
    j["inclination"] = degreesToJson(f.inclination);
    j["magnetic_azimuth"] = optionalAngleToJson(f.magnetic_azimuth);
    j["true_azimuth"] = optionalAngleToJson(f.true_azimuth);
    j["shift"] = metersToJson(f.shift);
    j["elongation"] = metersToJson(f.elongation);
    j["x"] = metersToJson(f.x);
    j["y"] = metersToJson(f.y);
    j["deviation"] = metersToJson(f.deviation);
    j["deviation_direction"] = degreesToJson(f.deviation_direction);
    j["tvd"] = metersToJson(f.tvd);
    j["intensity_10m"] = f.intensity_10m;
    j["intensity_L"] = f.intensity_L;
    return j;
}

ProjectPointFactual projectPointFactualFromJson(const json& j) {
    ProjectPointFactual f;
    f.inclination = degreesFromJson(j.value("inclination", json(0.0)));
    f.magnetic_azimuth = optionalAngleFromJson(j.value("magnetic_azimuth", json(nullptr)));
    f.true_azimuth = optionalAngleFromJson(j.value("true_azimuth", json(nullptr)));
    f.shift = metersFromJson(j.value("shift", json(0.0)));
    f.elongation = metersFromJson(j.value("elongation", json(0.0)));
    f.x = metersFromJson(j.value("x", json(0.0)));
    f.y = metersFromJson(j.value("y", json(0.0)));
    f.deviation = metersFromJson(j.value("deviation", json(0.0)));
    f.deviation_direction = degreesFromJson(j.value("deviation_direction", json(0.0)));
    f.tvd = metersFromJson(j.value("tvd", json(0.0)));
    f.intensity_10m = j.value("intensity_10m", 0.0);
    f.intensity_L = j.value("intensity_L", 0.0);
    return f;
}

json projectPointToJson(const ProjectPoint& pp) {
    json j;
    j["name"] = pp.name;
    j["azimuth_geographic"] = optionalAngleToJson(pp.azimuth_geographic);
    j["shift"] = metersToJson(pp.shift);
    j["depth"] = pp.depth.has_value() ? metersToJson(*pp.depth) : json(nullptr);
    j["abs_depth"] = pp.abs_depth.has_value() ? metersToJson(*pp.abs_depth) : json(nullptr);
    j["radius"] = metersToJson(pp.radius);
    j["base_shift"] = pp.base_shift.has_value() ? metersToJson(*pp.base_shift) : json(nullptr);
    j["base_azimuth"] = pp.base_azimuth.has_value()
        ? optionalAngleToJson(pp.base_azimuth.value())
        : json(nullptr);
    j["base_depth"] = pp.base_depth.has_value() ? metersToJson(*pp.base_depth) : json(nullptr);
    if (pp.factual.has_value()) {
        j["factual"] = projectPointFactualToJson(*pp.factual);
    } else {
        j["factual"] = nullptr;
    }
    return j;
}

ProjectPoint projectPointFromJson(const json& j) {
    ProjectPoint pp;
    pp.name = j.value("name", "");
    pp.azimuth_geographic = optionalAngleFromJson(j.value("azimuth_geographic", json(nullptr)));
    pp.shift = metersFromJson(j.value("shift", json(0.0)));
    if (j.contains("depth") && !j.at("depth").is_null()) {
        pp.depth = metersFromJson(j.at("depth"));
    }
    if (j.contains("abs_depth") && !j.at("abs_depth").is_null()) {
        pp.abs_depth = metersFromJson(j.at("abs_depth"));
    }
    pp.radius = metersFromJson(j.value("radius", json(50.0)));
    if (j.contains("base_shift") && !j.at("base_shift").is_null()) {
        pp.base_shift = metersFromJson(j.at("base_shift"));
    }
    if (j.contains("base_azimuth") && !j.at("base_azimuth").is_null()) {
        pp.base_azimuth = optionalAngleFromJson(j.at("base_azimuth"));
    }
    if (j.contains("base_depth") && !j.at("base_depth").is_null()) {
        pp.base_depth = metersFromJson(j.at("base_depth"));
    }
    if (j.contains("factual") && !j.at("factual").is_null()) {
        pp.factual = projectPointFactualFromJson(j.at("factual"));
    }
    return pp;
}

json shotPointToJson(const ShotPoint& sp) {
    json j;
    j["azimuth_geographic"] = optionalAngleToJson(sp.azimuth_geographic);
    j["shift"] = metersToJson(sp.shift);
    j["ground_altitude"] = metersToJson(sp.ground_altitude);
    j["number"] = sp.number;
    j["color"] = sp.color.has_value() ? colorToJson(*sp.color) : json(nullptr);
    return j;
}

ShotPoint shotPointFromJson(const json& j) {
    ShotPoint sp;
    sp.azimuth_geographic = optionalAngleFromJson(j.value("azimuth_geographic", json(nullptr)));
    sp.shift = metersFromJson(j.value("shift", json(0.0)));
    sp.ground_altitude = metersFromJson(j.value("ground_altitude", json(0.0)));
    sp.number = j.value("number", "");
    if (j.contains("color") && !j.at("color").is_null()) {
        sp.color = colorFromJson(j.at("color"));
    }
    return sp;
}

json clusterPositionToJson(const ClusterPosition& pos) {
    if (std::holds_alternative<std::monostate>(pos.position)) {
        return json(nullptr);
    }
    if (const auto* az_shift = std::get_if<std::pair<OptionalAngle, Meters>>(&pos.position)) {
        json j;
        j["type"] = "azimuth_shift";
        j["azimuth"] = optionalAngleToJson(az_shift->first);
        j["shift"] = metersToJson(az_shift->second);
        return j;
    }
    if (const auto* xy = std::get_if<std::pair<Meters, Meters>>(&pos.position)) {
        json j;
        j["type"] = "xy";
        j["x"] = metersToJson(xy->first);
        j["y"] = metersToJson(xy->second);
        return j;
    }
    return json(nullptr);
}

ClusterPosition clusterPositionFromJson(const json& j) {
    ClusterPosition pos;
    if (j.is_null()) {
        return pos;
    }

    std::string type = j.value("type", "");
    if (type == "azimuth_shift") {
        OptionalAngle az = optionalAngleFromJson(j.value("azimuth", json(nullptr)));
        Meters shift = metersFromJson(j.value("shift", json(0.0)));
        pos.position = std::make_pair(az, shift);
    } else if (type == "xy") {
        Meters x = metersFromJson(j.value("x", json(0.0)));
        Meters y = metersFromJson(j.value("y", json(0.0)));
        pos.position = std::make_pair(x, y);
    }

    return pos;
}

// === Сериализация перечислений ===

std::string azimuthModeToString(AzimuthMode mode) {
    switch (mode) {
        case AzimuthMode::Magnetic: return "magnetic";
        case AzimuthMode::True: return "true";
        case AzimuthMode::Auto: return "auto";
    }
    return "auto";
}

AzimuthMode azimuthModeFromString(const std::string& s) {
    if (s == "magnetic") return AzimuthMode::Magnetic;
    if (s == "true") return AzimuthMode::True;
    return AzimuthMode::Auto;
}

std::string trajectoryMethodToString(TrajectoryMethod method) {
    switch (method) {
        case TrajectoryMethod::AverageAngle: return "average_angle";
        case TrajectoryMethod::BalancedTangential: return "balanced_tangential";
        case TrajectoryMethod::MinimumCurvature: return "minimum_curvature";
        case TrajectoryMethod::MinimumCurvatureIntegral: return "minimum_curvature_integral";
        case TrajectoryMethod::RingArc: return "ring_arc";
    }
    return "minimum_curvature";
}

TrajectoryMethod trajectoryMethodFromString(const std::string& s) {
    if (s == "average_angle") return TrajectoryMethod::AverageAngle;
    if (s == "balanced_tangential") return TrajectoryMethod::BalancedTangential;
    if (s == "minimum_curvature_integral") return TrajectoryMethod::MinimumCurvatureIntegral;
    if (s == "ring_arc") return TrajectoryMethod::RingArc;
    return TrajectoryMethod::MinimumCurvature;
}

std::string headerStyleToString(HeaderStyle style) {
    switch (style) {
        case HeaderStyle::None: return "none";
        case HeaderStyle::Compact: return "compact";
        case HeaderStyle::Full: return "full";
    }
    return "compact";
}

HeaderStyle headerStyleFromString(const std::string& s) {
    if (s == "none") return HeaderStyle::None;
    if (s == "full") return HeaderStyle::Full;
    return HeaderStyle::Compact;
}

// === Сериализация MeasurementPoint ===

json measurementPointToJson(const MeasurementPoint& m) {
    json j;
    j["depth"] = metersToJson(m.depth);
    j["inclination"] = degreesToJson(m.inclination);
    j["magnetic_azimuth"] = optionalAngleToJson(m.magnetic_azimuth);
    j["true_azimuth"] = optionalAngleToJson(m.true_azimuth);
    if (m.rotation.has_value()) {
        j["rotation"] = *m.rotation;
    } else {
        j["rotation"] = nullptr;
    }
    if (m.rop.has_value()) {
        j["rop"] = *m.rop;
    }
    if (m.marker.has_value()) {
        j["marker"] = *m.marker;
    }
    return j;
}

MeasurementPoint measurementPointFromJson(const json& j) {
    MeasurementPoint m;
    m.depth = metersFromJson(j.at("depth"));
    m.inclination = degreesFromJson(j.at("inclination"));

    if (j.contains("magnetic_azimuth")) {
        m.magnetic_azimuth = optionalAngleFromJson(j.at("magnetic_azimuth"));
    }
    if (j.contains("true_azimuth")) {
        m.true_azimuth = optionalAngleFromJson(j.at("true_azimuth"));
    }
    if (j.contains("rotation") && !j.at("rotation").is_null()) {
        m.rotation = j.at("rotation").get<double>();
    }
    if (j.contains("rop") && !j.at("rop").is_null()) {
        m.rop = j.at("rop").get<double>();
    }
    if (j.contains("marker") && !j.at("marker").is_null()) {
        m.marker = j.at("marker").get<std::string>();
    }

    return m;
}

// === Сериализация ProcessedPoint ===

json processedPointToJson(const ProcessedPoint& p) {
    json j;
    j["depth"] = metersToJson(p.depth);
    j["inclination"] = degreesToJson(p.inclination);
    j["magnetic_azimuth"] = optionalAngleToJson(p.magnetic_azimuth);
    j["true_azimuth"] = optionalAngleToJson(p.true_azimuth);
    j["computed_azimuth"] = optionalAngleToJson(p.computed_azimuth);
    if (p.rotation.has_value()) {
        j["rotation"] = *p.rotation;
    } else {
        j["rotation"] = nullptr;
    }
    j["x"] = metersToJson(p.x);
    j["y"] = metersToJson(p.y);
    j["tvd"] = metersToJson(p.tvd);
    j["absg"] = metersToJson(p.absg);
    j["shift"] = metersToJson(p.shift);
    j["direction_angle"] = degreesToJson(p.direction_angle);
    j["elongation"] = metersToJson(p.elongation);
    j["intensity_10m"] = p.intensity_10m;
    j["intensity_L"] = p.intensity_L;
    j["error_x"] = metersToJson(p.error_x);
    j["error_y"] = metersToJson(p.error_y);
    j["error_absg"] = metersToJson(p.error_absg);
    j["error_intensity"] = p.error_intensity;

    if (p.rop.has_value()) j["rop"] = *p.rop;
    if (p.marker.has_value()) j["marker"] = *p.marker;

    return j;
}

ProcessedPoint processedPointFromJson(const json& j) {
    ProcessedPoint p;
    p.depth = metersFromJson(j.at("depth"));
    p.inclination = degreesFromJson(j.at("inclination"));
    p.magnetic_azimuth = optionalAngleFromJson(j.value("magnetic_azimuth", json(nullptr)));
    p.true_azimuth = optionalAngleFromJson(j.value("true_azimuth", json(nullptr)));
    p.computed_azimuth = optionalAngleFromJson(j.value("computed_azimuth", json(nullptr)));
    if (j.contains("rotation") && !j.at("rotation").is_null()) {
        p.rotation = j.at("rotation").get<double>();
    }
    p.x = metersFromJson(j.at("x"));
    p.y = metersFromJson(j.at("y"));
    p.tvd = metersFromJson(j.at("tvd"));
    p.absg = metersFromJson(j.at("absg"));
    p.shift = metersFromJson(j.at("shift"));
    p.direction_angle = degreesFromJson(j.at("direction_angle"));
    p.elongation = metersFromJson(j.at("elongation"));
    p.intensity_10m = j.at("intensity_10m").get<double>();
    p.intensity_L = j.at("intensity_L").get<double>();
    p.error_x = metersFromJson(j.value("error_x", json(0.0)));
    p.error_y = metersFromJson(j.value("error_y", json(0.0)));
    p.error_absg = metersFromJson(j.value("error_absg", json(0.0)));
    p.error_intensity = j.value("error_intensity", 0.0);

    if (j.contains("rop") && !j.at("rop").is_null()) {
        p.rop = j.at("rop").get<double>();
    }
    if (j.contains("marker") && !j.at("marker").is_null()) {
        p.marker = j.at("marker").get<std::string>();
    }

    return p;
}

// === Сериализация IntervalData ===

json intervalDataToJson(const IntervalData& d) {
    json j;

    // Метаданные
    j["uwi"] = d.uwi;
    j["region"] = d.region;
    j["field"] = d.field;
    j["area"] = d.area;
    j["cluster"] = d.cluster;
    j["well"] = d.well;
    j["customer"] = d.customer;
    j["contractor"] = d.contractor;
    j["study_date"] = d.study_date;
    j["study_type"] = d.study_type;

    // Параметры
    j["rotor_table_altitude"] = metersToJson(d.rotor_table_altitude);
    j["ground_altitude"] = metersToJson(d.ground_altitude);
    j["magnetic_declination"] = degreesToJson(d.magnetic_declination);
    j["conductor_shoe"] = metersToJson(d.conductor_shoe);
    j["target_bottom"] = metersToJson(d.target_bottom);
    j["current_bottom"] = metersToJson(d.current_bottom);
    j["interval_start"] = metersToJson(d.interval_start);
    j["interval_end"] = metersToJson(d.interval_end);
    j["angle_measurement_error"] = degreesToJson(d.angle_measurement_error);
    j["azimuth_measurement_error"] = degreesToJson(d.azimuth_measurement_error);

    // Замеры
    json measurements = json::array();
    for (const auto& m : d.measurements) {
        measurements.push_back(measurementPointToJson(m));
    }
    j["measurements"] = measurements;

    return j;
}

IntervalData intervalDataFromJson(const json& j) {
    IntervalData d;

    d.uwi = j.value("uwi", "");
    d.region = j.value("region", "");
    d.field = j.value("field", "");
    d.area = j.value("area", "");
    d.cluster = j.value("cluster", "");
    d.well = j.value("well", "");
    d.customer = j.value("customer", "");
    d.contractor = j.value("contractor", "");
    d.study_date = j.value("study_date", "");
    d.study_type = j.value("study_type", "");

    d.rotor_table_altitude = metersFromJson(j.value("rotor_table_altitude", json(0.0)));
    d.ground_altitude = metersFromJson(j.value("ground_altitude", json(0.0)));
    d.magnetic_declination = degreesFromJson(j.value("magnetic_declination", json(0.0)));
    d.conductor_shoe = metersFromJson(j.value("conductor_shoe", json(0.0)));
    d.target_bottom = metersFromJson(j.value("target_bottom", json(0.0)));
    d.current_bottom = metersFromJson(j.value("current_bottom", json(0.0)));
    d.interval_start = metersFromJson(j.value("interval_start", json(0.0)));
    d.interval_end = metersFromJson(j.value("interval_end", json(0.0)));
    d.angle_measurement_error = degreesFromJson(j.value("angle_measurement_error", json(0.5)));
    d.azimuth_measurement_error = degreesFromJson(j.value("azimuth_measurement_error", json(2.0)));

    if (j.contains("measurements")) {
        for (const auto& mj : j.at("measurements")) {
            d.measurements.push_back(measurementPointFromJson(mj));
        }
    }

    return d;
}

// === Сериализация WellResult ===

json wellResultToJson(const WellResult& r) {
    json j;

    // Метаданные (копии из IntervalData)
    j["uwi"] = r.uwi;
    j["region"] = r.region;
    j["field"] = r.field;
    j["area"] = r.area;
    j["cluster"] = r.cluster;
    j["well"] = r.well;
    j["rotor_table_altitude"] = metersToJson(r.rotor_table_altitude);
    j["ground_altitude"] = metersToJson(r.ground_altitude);
    j["magnetic_declination"] = degreesToJson(r.magnetic_declination);
    j["target_bottom"] = metersToJson(r.target_bottom);
    j["current_bottom"] = metersToJson(r.current_bottom);

    // Параметры обработки
    j["azimuth_mode"] = azimuthModeToString(r.azimuth_mode);
    j["trajectory_method"] = trajectoryMethodToString(r.trajectory_method);
    j["intensity_interval_L"] = metersToJson(r.intensity_interval_L);

    // Статистика
    j["max_inclination"] = degreesToJson(r.max_inclination);
    j["max_inclination_depth"] = metersToJson(r.max_inclination_depth);
    j["max_intensity_10m"] = r.max_intensity_10m;
    j["max_intensity_10m_depth"] = metersToJson(r.max_intensity_10m_depth);
    j["max_intensity_L"] = r.max_intensity_L;
    j["max_intensity_L_depth"] = metersToJson(r.max_intensity_L_depth);
    j["actual_shift"] = metersToJson(r.actual_shift);
    j["actual_direction_angle"] = degreesToJson(r.actual_direction_angle);

    // Точки
    json points = json::array();
    for (const auto& p : r.points) {
        points.push_back(processedPointToJson(p));
    }
    j["points"] = points;

    json project_points = json::array();
    for (const auto& pp : r.project_points) {
        project_points.push_back(projectPointToJson(pp));
    }
    j["project_points"] = project_points;

    return j;
}

WellResult wellResultFromJson(const json& j) {
    WellResult r;

    r.uwi = j.value("uwi", "");
    r.region = j.value("region", "");
    r.field = j.value("field", "");
    r.area = j.value("area", "");
    r.cluster = j.value("cluster", "");
    r.well = j.value("well", "");
    r.rotor_table_altitude = metersFromJson(j.value("rotor_table_altitude", json(0.0)));
    r.ground_altitude = metersFromJson(j.value("ground_altitude", json(0.0)));
    r.magnetic_declination = degreesFromJson(j.value("magnetic_declination", json(0.0)));
    r.target_bottom = metersFromJson(j.value("target_bottom", json(0.0)));
    r.current_bottom = metersFromJson(j.value("current_bottom", json(0.0)));

    r.azimuth_mode = azimuthModeFromString(j.value("azimuth_mode", "auto"));
    r.trajectory_method = trajectoryMethodFromString(j.value("trajectory_method", "minimum_curvature"));
    r.intensity_interval_L = metersFromJson(j.value("intensity_interval_L", json(25.0)));

    r.max_inclination = degreesFromJson(j.value("max_inclination", json(0.0)));
    r.max_inclination_depth = metersFromJson(j.value("max_inclination_depth", json(0.0)));
    r.max_intensity_10m = j.value("max_intensity_10m", 0.0);
    r.max_intensity_10m_depth = metersFromJson(j.value("max_intensity_10m_depth", json(0.0)));
    r.max_intensity_L = j.value("max_intensity_L", 0.0);
    r.max_intensity_L_depth = metersFromJson(j.value("max_intensity_L_depth", json(0.0)));
    r.actual_shift = metersFromJson(j.value("actual_shift", json(0.0)));
    r.actual_direction_angle = degreesFromJson(j.value("actual_direction_angle", json(0.0)));

    if (j.contains("points")) {
        for (const auto& pj : j.at("points")) {
            r.points.push_back(processedPointFromJson(pj));
        }
    }

    if (j.contains("project_points")) {
        for (const auto& ppj : j.at("project_points")) {
            r.project_points.push_back(projectPointFromJson(ppj));
        }
    }

    return r;
}

// === Сериализация WellEntry ===

json wellEntryToJson(const WellEntry& e) {
    json j;
    j["id"] = e.id;
    j["source_data"] = intervalDataToJson(e.source_data);
    if (e.result.has_value()) {
        j["result"] = wellResultToJson(*e.result);
    } else {
        j["result"] = nullptr;
    }
    j["visible"] = e.visible;
    j["is_base"] = e.is_base;
    j["color"] = colorToJson(e.color);

    ProjectPointList project_points = e.project_points;
    if (project_points.empty() && e.result.has_value()) {
        project_points = e.result->project_points;
    }

    json pp_array = json::array();
    for (const auto& pp : project_points) {
        pp_array.push_back(projectPointToJson(pp));
    }
    j["project_points"] = pp_array;

    j["cluster_position"] = clusterPositionToJson(e.cluster_position);

    json shot_array = json::array();
    for (const auto& sp : e.shot_points) {
        shot_array.push_back(shotPointToJson(sp));
    }
    j["shot_points"] = shot_array;
    return j;
}

WellEntry wellEntryFromJson(const json& j) {
    WellEntry e;
    e.id = j.value("id", "");
    e.source_data = intervalDataFromJson(j.at("source_data"));
    if (j.contains("result") && !j.at("result").is_null()) {
        e.result = wellResultFromJson(j.at("result"));
    }
    e.visible = j.value("visible", true);
    e.is_base = j.value("is_base", false);
    e.color = colorFromJson(j.value("color", json("#0000FF")));
    if (j.contains("project_points")) {
        for (const auto& ppj : j.at("project_points")) {
            e.project_points.push_back(projectPointFromJson(ppj));
        }
    }

    if (j.contains("cluster_position")) {
        e.cluster_position = clusterPositionFromJson(j.at("cluster_position"));
    }

    if (j.contains("shot_points")) {
        for (const auto& spj : j.at("shot_points")) {
            e.shot_points.push_back(shotPointFromJson(spj));
        }
    }

    if (e.project_points.empty() && e.result.has_value()) {
        e.project_points = e.result->project_points;
    }
    return e;
}

// === Сериализация настроек визуализации ===

json axonometrySettingsToJson(const AxonometrySettings& s) {
    json j;
    j["rotation_x"] = s.rotation_x;
    j["rotation_z"] = s.rotation_z;
    j["zoom"] = s.zoom;
    j["pan_x"] = s.pan_x;
    j["pan_y"] = s.pan_y;
    j["pan_z"] = s.pan_z;
    j["show_grid_horizontal"] = s.show_grid_horizontal;
    j["show_grid_vertical"] = s.show_grid_vertical;
    j["show_grid_plan"] = s.show_grid_plan;
    j["grid_horizontal_depth"] = metersToJson(s.grid_horizontal_depth);
    j["grid_interval"] = metersToJson(s.grid_interval);
    j["show_sea_level"] = s.show_sea_level;
    j["show_axes"] = s.show_axes;
    j["show_depth_labels"] = s.show_depth_labels;
    j["depth_label_interval"] = metersToJson(s.depth_label_interval);
    j["show_well_labels"] = s.show_well_labels;
    j["background_color"] = colorToJson(s.background_color);
    j["grid_color"] = colorToJson(s.grid_color);
    j["sea_level_color"] = colorToJson(s.sea_level_color);
    j["trajectory_line_width"] = s.trajectory_line_width;
    j["grid_line_width"] = s.grid_line_width;
    return j;
}

AxonometrySettings axonometrySettingsFromJson(const json& j) {
    AxonometrySettings s;
    s.rotation_x = j.value("rotation_x", 30.0f);
    s.rotation_z = j.value("rotation_z", 45.0f);
    s.zoom = j.value("zoom", 1.0f);
    s.pan_x = j.value("pan_x", 0.0f);
    s.pan_y = j.value("pan_y", 0.0f);
    s.pan_z = j.value("pan_z", 0.0f);
    s.show_grid_horizontal = j.value("show_grid_horizontal", true);
    s.show_grid_vertical = j.value("show_grid_vertical", true);
    s.show_grid_plan = j.value("show_grid_plan", false);
    s.grid_horizontal_depth = metersFromJson(j.value("grid_horizontal_depth", json(0.0)));
    s.grid_interval = metersFromJson(j.value("grid_interval", json(100.0)));
    s.show_sea_level = j.value("show_sea_level", true);
    s.show_axes = j.value("show_axes", true);
    s.show_depth_labels = j.value("show_depth_labels", true);
    s.depth_label_interval = metersFromJson(j.value("depth_label_interval", json(100.0)));
    s.show_well_labels = j.value("show_well_labels", true);
    s.background_color = colorFromJson(j.value("background_color", json("#FFFFFF")));
    s.grid_color = colorFromJson(j.value("grid_color", json("#D3D3D3")));
    s.sea_level_color = colorFromJson(j.value("sea_level_color", json("#ADD8E6")));
    s.trajectory_line_width = j.value("trajectory_line_width", 2.0f);
    s.grid_line_width = j.value("grid_line_width", 1.0f);
    return s;
}

json planSettingsToJson(const PlanSettings& s) {
    json j;
    j["scale"] = s.scale;
    j["pan_x"] = s.pan_x;
    j["pan_y"] = s.pan_y;
    j["show_grid"] = s.show_grid;
    j["grid_interval"] = metersToJson(s.grid_interval);
    j["show_project_points"] = s.show_project_points;
    j["show_tolerance_circles"] = s.show_tolerance_circles;
    j["show_deviation_triangles"] = s.show_deviation_triangles;
    j["show_scale_bar"] = s.show_scale_bar;
    j["show_north_arrow"] = s.show_north_arrow;
    j["show_well_labels"] = s.show_well_labels;
    j["show_depth_labels"] = s.show_depth_labels;
    j["background_color"] = colorToJson(s.background_color);
    j["grid_color"] = colorToJson(s.grid_color);
    j["trajectory_line_width"] = s.trajectory_line_width;
    j["grid_line_width"] = s.grid_line_width;
    return j;
}

PlanSettings planSettingsFromJson(const json& j) {
    PlanSettings s;
    s.scale = j.value("scale", 1.0f);
    s.pan_x = j.value("pan_x", 0.0f);
    s.pan_y = j.value("pan_y", 0.0f);
    s.show_grid = j.value("show_grid", true);
    s.grid_interval = metersFromJson(j.value("grid_interval", json(100.0)));
    s.show_project_points = j.value("show_project_points", true);
    s.show_tolerance_circles = j.value("show_tolerance_circles", true);
    s.show_deviation_triangles = j.value("show_deviation_triangles", true);
    s.show_scale_bar = j.value("show_scale_bar", true);
    s.show_north_arrow = j.value("show_north_arrow", true);
    s.show_well_labels = j.value("show_well_labels", true);
    s.show_depth_labels = j.value("show_depth_labels", false);
    s.background_color = colorFromJson(j.value("background_color", json("#FFFFFF")));
    s.grid_color = colorFromJson(j.value("grid_color", json("#D3D3D3")));
    s.trajectory_line_width = j.value("trajectory_line_width", 2.0f);
    s.grid_line_width = j.value("grid_line_width", 1.0f);
    return s;
}

json verticalSettingsToJson(const VerticalProjectionSettings& s) {
    json j;
    if (s.plane_azimuth.has_value()) {
        j["plane_azimuth"] = degreesToJson(*s.plane_azimuth);
    } else {
        j["plane_azimuth"] = nullptr;
    }
    j["auto_plane"] = s.auto_plane;
    j["scale_horizontal"] = s.scale_horizontal;
    j["scale_vertical"] = s.scale_vertical;
    j["pan_x"] = s.pan_x;
    j["pan_y"] = s.pan_y;
    j["show_grid"] = s.show_grid;
    j["grid_interval_horizontal"] = metersToJson(s.grid_interval_horizontal);
    j["grid_interval_vertical"] = metersToJson(s.grid_interval_vertical);
    j["show_header"] = s.show_header;
    j["header_style"] = headerStyleToString(s.header_style);
    j["show_sea_level"] = s.show_sea_level;
    j["show_depth_labels"] = s.show_depth_labels;
    j["show_well_labels"] = s.show_well_labels;
    j["background_color"] = colorToJson(s.background_color);
    j["grid_color"] = colorToJson(s.grid_color);
    j["sea_level_color"] = colorToJson(s.sea_level_color);
    j["trajectory_line_width"] = s.trajectory_line_width;
    j["grid_line_width"] = s.grid_line_width;
    return j;
}

VerticalProjectionSettings verticalSettingsFromJson(const json& j) {
    VerticalProjectionSettings s;
    if (j.contains("plane_azimuth") && !j.at("plane_azimuth").is_null()) {
        s.plane_azimuth = degreesFromJson(j.at("plane_azimuth"));
    }
    s.auto_plane = j.value("auto_plane", true);
    s.scale_horizontal = j.value("scale_horizontal", 1.0f);
    s.scale_vertical = j.value("scale_vertical", 1.0f);
    s.pan_x = j.value("pan_x", 0.0f);
    s.pan_y = j.value("pan_y", 0.0f);
    s.show_grid = j.value("show_grid", true);
    s.grid_interval_horizontal = metersFromJson(j.value("grid_interval_horizontal", json(100.0)));
    s.grid_interval_vertical = metersFromJson(j.value("grid_interval_vertical", json(100.0)));
    s.show_header = j.value("show_header", true);
    s.header_style = headerStyleFromString(j.value("header_style", "compact"));
    s.show_sea_level = j.value("show_sea_level", true);
    s.show_depth_labels = j.value("show_depth_labels", true);
    s.show_well_labels = j.value("show_well_labels", true);
    s.background_color = colorFromJson(j.value("background_color", json("#FFFFFF")));
    s.grid_color = colorFromJson(j.value("grid_color", json("#D3D3D3")));
    s.sea_level_color = colorFromJson(j.value("sea_level_color", json("#ADD8E6")));
    s.trajectory_line_width = j.value("trajectory_line_width", 2.0f);
    s.grid_line_width = j.value("grid_line_width", 1.0f);
    return s;
}

std::string azimuthInterpretationToString(AzimuthInterpretation interp) {
    switch (interp) {
        case AzimuthInterpretation::Geographic: return "geographic";
        case AzimuthInterpretation::Directional: return "directional";
    }
    return "geographic";
}

AzimuthInterpretation azimuthInterpretationFromString(const std::string& s) {
    if (s == "directional") return AzimuthInterpretation::Directional;
    return AzimuthInterpretation::Geographic;
}

std::string doglegMethodToString(DoglegMethod method) {
    switch (method) {
        case DoglegMethod::Cosine: return "cosine";
        case DoglegMethod::Sine: return "sine";
    }
    return "sine";
}

DoglegMethod doglegMethodFromString(const std::string& s) {
    if (s == "cosine") return DoglegMethod::Cosine;
    return DoglegMethod::Sine;
}

json verticalityConfigToJson(const VerticalityConfig& v) {
    json j;
    j["critical_inclination"] = degreesToJson(v.critical_inclination);
    j["near_surface_depth"] = metersToJson(v.near_surface_depth);
    return j;
}

VerticalityConfig verticalityConfigFromJson(const json& j) {
    VerticalityConfig v;
    v.critical_inclination = degreesFromJson(j.value("critical_inclination", json(0.5)));
    v.near_surface_depth = metersFromJson(j.value("near_surface_depth", json(30.0)));
    return v;
}

json processingSettingsToJson(const ProcessingSettings& s) {
    json j;
    j["trajectory_method"] = trajectoryMethodToString(s.trajectory_method);
    j["azimuth_mode"] = azimuthModeToString(s.azimuth_mode);
    j["azimuth_interpretation"] = azimuthInterpretationToString(s.azimuth_interpretation);
    j["dogleg_method"] = doglegMethodToString(s.dogleg_method);
    j["intensity_interval_L"] = metersToJson(s.intensity_interval_L);
    j["verticality"] = verticalityConfigToJson(s.verticality);
    j["smooth_intensity"] = s.smooth_intensity;
    j["interpolate_missing_azimuths"] = s.interpolate_missing_azimuths;
    j["extend_last_azimuth"] = s.extend_last_azimuth;
    j["blank_vertical_azimuth"] = s.blank_vertical_azimuth;
    j["vertical_if_no_azimuth"] = s.vertical_if_no_azimuth;
    return j;
}

ProcessingSettings processingSettingsFromJson(const json& j) {
    ProcessingSettings s;
    s.trajectory_method = trajectoryMethodFromString(j.value("trajectory_method", "minimum_curvature"));
    s.azimuth_mode = azimuthModeFromString(j.value("azimuth_mode", "auto"));
    s.azimuth_interpretation = azimuthInterpretationFromString(j.value("azimuth_interpretation", "geographic"));
    s.dogleg_method = doglegMethodFromString(j.value("dogleg_method", "sine"));
    s.intensity_interval_L = metersFromJson(j.value("intensity_interval_L", json(25.0)));
    if (j.contains("verticality")) {
        s.verticality = verticalityConfigFromJson(j.at("verticality"));
    }
    s.smooth_intensity = j.value("smooth_intensity", true);
    s.interpolate_missing_azimuths = j.value("interpolate_missing_azimuths", false);
    s.extend_last_azimuth = j.value("extend_last_azimuth", false);
    s.blank_vertical_azimuth = j.value("blank_vertical_azimuth", true);
    s.vertical_if_no_azimuth = j.value("vertical_if_no_azimuth", true);
    return s;
}

// === Сериализация Project ===

json projectToJsonInternal(const Project& p) {
    json j;

    // Заголовок
    j["version"] = PROJECT_FORMAT_VERSION;
    j["format"] = PROJECT_FORMAT_ID;

    // Метаданные
    json metadata;
    metadata["name"] = p.name;
    metadata["description"] = p.description;
    metadata["created"] = p.created_date;
    metadata["modified"] = p.modified_date;
    metadata["author"] = p.author;
    j["metadata"] = metadata;

    // Скважины
    json wells = json::array();
    for (const auto& w : p.wells) {
        wells.push_back(wellEntryToJson(w));
    }
    j["wells"] = wells;

    // Настройки визуализации
    json settings;
    settings["axonometry"] = axonometrySettingsToJson(p.axonometry);
    settings["plan"] = planSettingsToJson(p.plan);
    settings["vertical"] = verticalSettingsToJson(p.vertical);
    settings["processing"] = processingSettingsToJson(p.processing);
    j["settings"] = settings;

    return j;
}

Project projectFromJsonInternal(const json& j) {
    Project p;

    // Проверка формата
    std::string format = j.value("format", "");
    if (format != PROJECT_FORMAT_ID) {
        throw ProjectError("Неверный формат файла проекта");
    }

    // Метаданные
    if (j.contains("metadata")) {
        const auto& meta = j.at("metadata");
        p.name = meta.value("name", "");
        p.description = meta.value("description", "");
        p.created_date = meta.value("created", "");
        p.modified_date = meta.value("modified", "");
        p.author = meta.value("author", "");
    }

    // Скважины
    if (j.contains("wells")) {
        for (const auto& wj : j.at("wells")) {
            p.wells.push_back(wellEntryFromJson(wj));
        }
    }

    // Настройки
    if (j.contains("settings")) {
        const auto& settings = j.at("settings");
        if (settings.contains("axonometry")) {
            p.axonometry = axonometrySettingsFromJson(settings.at("axonometry"));
        }
        if (settings.contains("plan")) {
            p.plan = planSettingsFromJson(settings.at("plan"));
        }
        if (settings.contains("vertical")) {
            p.vertical = verticalSettingsFromJson(settings.at("vertical"));
        }
        if (settings.contains("processing")) {
            p.processing = processingSettingsFromJson(settings.at("processing"));
        }
    }

    return p;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream ss;
    ss << (1900 + tm.tm_year) << "-"
       << std::setfill('0') << std::setw(2) << (tm.tm_mon + 1) << "-"
       << std::setw(2) << tm.tm_mday << "T"
       << std::setw(2) << tm.tm_hour << ":"
       << std::setw(2) << tm.tm_min << ":"
       << std::setw(2) << tm.tm_sec;
    return ss.str();
}

} // anonymous namespace

bool isProjectFile(const std::filesystem::path& path) noexcept {
    try {
        if (!std::filesystem::exists(path)) {
            return false;
        }

        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (ext != ".inclproj") {
            return false;
        }

        // Проверяем содержимое
        std::ifstream file(path);
        if (!file) return false;

        json j;
        try {
            file >> j;
        } catch (...) {
            return false;
        }

        return j.value("format", "") == PROJECT_FORMAT_ID;
    } catch (...) {
        return false;
    }
}

std::string getProjectVersion(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream file(path);
        if (!file) return "";

        json j;
        file >> j;

        return j.value("version", "");
    } catch (...) {
        return "";
    }
}

Project loadProject(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw ProjectError("Не удалось открыть файл: " + path.string());
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw ProjectError("Ошибка парсинга JSON: " + std::string(e.what()));
    }

    Project p = projectFromJsonInternal(j);
    p.file_path = path.string();
    return p;
}

void saveProject(const Project& project, const std::filesystem::path& path) {
    // Обновляем время модификации
    Project p = project;
    p.modified_date = getCurrentTimestamp();
    if (p.created_date.empty()) {
        p.created_date = p.modified_date;
    }
    p.file_path = path.string();

    json j = projectToJsonInternal(p);

    // Атомарная запись
    std::filesystem::path temp_path = path;
    temp_path += ".tmp";

    std::ofstream file(temp_path);
    if (!file) {
        throw ProjectError("Не удалось создать файл: " + path.string());
    }

    file << j.dump(2);  // С отступами для читаемости
    file.close();

    try {
        std::filesystem::rename(temp_path, path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::filesystem::remove(temp_path);
        throw ProjectError("Ошибка сохранения файла: " + std::string(e.what()));
    }
}

std::string projectToJson(const Project& project, int indent) {
    return projectToJsonInternal(project).dump(indent);
}

Project projectFromJson(const std::string& json_str) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        throw ProjectError("Ошибка парсинга JSON: " + std::string(e.what()));
    }

    return projectFromJsonInternal(j);
}

} // namespace incline::io

#include "SketchDocumentJson.h"

#include <stdexcept>

// NOTE: JSON (de)serialization uses ADL.
// The functions are placed in the namespace of their associated types.

namespace {
    template<typename T>
    T require(const nlohmann::json& j, const char* field) {
        if (!j.contains(field)) {
            throw std::runtime_error(std::string("Missing field: ") + field);
        }
        return j.at(field).get<T>();
    }
}

namespace domain::sketch {

    void to_json(nlohmann::json& j, const Vec2& v) {
        j = nlohmann::json{ {"x", v.x}, {"y", v.y} };
    }

    void from_json(const nlohmann::json& j, Vec2& v) {
        v.x = require<double>(j, "x");
        v.y = require<double>(j, "y");
    }

}

namespace adapters::persistence::dto {

    static void headerToJson(json& j, const domain::sketch::EntityId id, const std::string& name, bool construction, bool visible, bool selectable) {
        j["id"] = id;
        if (!name.empty()) j["name"] = name;
        if (construction) j["construction"] = construction;
        if (!visible) j["visible"] = visible;
        if (!selectable) j["selectable"] = selectable;
    }

    static void headerFromJson(const json& j, domain::sketch::EntityId& id, std::string& name, bool& construction, bool& visible, bool& selectable) {
        id = require<domain::sketch::EntityId>(j, "id");
        name = j.value("name", "");
        construction = j.value("construction", false);
        visible = j.value("visible", true);
        selectable = j.value("selectable", true);
    }

    void to_json(json& j, const EntityRefDto& r) {
        j = json{ {"id", r.id}, {"anchor", static_cast<int>(r.anchor)} };
    }
    void from_json(const json& j, EntityRefDto& r) {
        r.id = require<domain::sketch::EntityId>(j, "id");
        r.anchor = static_cast<domain::sketch::EntityAnchor>(require<int>(j, "anchor"));
    }

    void to_json(json& j, const ConstraintMetaDto& m) {
        j = json{ {"id", m.id}, {"name", m.name}, {"enabled", m.enabled}, {"suppressed", m.suppressed} };
    }
    void from_json(const json& j, ConstraintMetaDto& m) {
        m.id = require<domain::sketch::ConstraintId>(j, "id");
        m.name = j.value("name", "");
        m.enabled = j.value("enabled", true);
        m.suppressed = j.value("suppressed", false);
    }

    // --- Entities ---
    void to_json(json& j, const PointDto& e) {
        j = json{};
        headerToJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        j["p"] = e.p;
    }
    void from_json(const json& j, PointDto& e) {
        headerFromJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        e.p = require<domain::sketch::Vec2>(j, "p");
    }

    void to_json(json& j, const LineDto& e) {
        j = json{};
        headerToJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        j["a"] = e.a;
        j["b"] = e.b;
    }
    void from_json(const json& j, LineDto& e) {
        headerFromJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        e.a = require<domain::sketch::Vec2>(j, "a");
        e.b = require<domain::sketch::Vec2>(j, "b");
    }

    void to_json(json& j, const CircleDto& e) {
        j = json{};
        headerToJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        j["center"] = e.center;
        j["radius"] = e.radius;
    }
    void from_json(const json& j, CircleDto& e) {
        headerFromJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        e.center = require<domain::sketch::Vec2>(j, "center");
        e.radius = require<double>(j, "radius");
    }

    void to_json(json& j, const ArcDto& e) {
        j = json{};
        headerToJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        j["center"] = e.center;
        j["radius"] = e.radius;
        j["start"] = e.start;
        j["end"] = e.end;
        j["ccw"] = e.ccw;
    }
    void from_json(const json& j, ArcDto& e) {
        headerFromJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        e.center = require<domain::sketch::Vec2>(j, "center");
        e.radius = require<double>(j, "radius");
        e.start = require<domain::sketch::Vec2>(j, "start");
        e.end = require<domain::sketch::Vec2>(j, "end");
        e.ccw = j.value("ccw", true);
    }

    void to_json(json& j, const EllipseDto& e) {
        j = json{};
        headerToJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        j["center"] = e.center;
        j["rx"] = e.rx;
        j["ry"] = e.ry;
        j["rotation"] = e.rotation;
    }
    void from_json(const json& j, EllipseDto& e) {
        headerFromJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        e.center = require<domain::sketch::Vec2>(j, "center");
        e.rx = require<double>(j, "rx");
        e.ry = require<double>(j, "ry");
        e.rotation = j.value("rotation", 0.0);
    }

    void to_json(json& j, const CurveDto& e) {
        j = json{};
        headerToJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        j["controlPoints"] = e.controlPoints;
        j["closed"] = e.closed;
    }
    void from_json(const json& j, CurveDto& e) {
        headerFromJson(j, e.id, e.name, e.construction, e.visible, e.selectable);
        e.controlPoints = j.value("controlPoints", std::vector<domain::sketch::Vec2>{});
        e.closed = j.value("closed", false);
    }

    // --- Constraints ---
    void to_json(json& j, const GeometricConstraintDto& c) {
        j = json{};
        j["meta"] = c.meta;
        j["type"] = static_cast<int>(c.type);
        j["refs"] = c.refs;
        if (c.param.has_value()) j["param"] = *c.param;
    }
    void from_json(const json& j, GeometricConstraintDto& c) {
        c.meta = require<ConstraintMetaDto>(j, "meta");
        c.type = static_cast<domain::sketch::GeometricConstraintType>(require<int>(j, "type"));
        c.refs = j.value("refs", std::vector<EntityRefDto>{});
        if (j.contains("param")) c.param = j.at("param").get<double>();
        else c.param.reset();
    }

    void to_json(json& j, const DimensionalConstraintDto& c) {
        j = json{};
        j["meta"] = c.meta;
        j["type"] = static_cast<int>(c.type);
        j["refs"] = c.refs;
        j["value"] = c.value;
        j["driving"] = c.driving;
        if (!c.units.empty()) j["units"] = c.units;
    }
    void from_json(const json& j, DimensionalConstraintDto& c) {
        c.meta = require<ConstraintMetaDto>(j, "meta");
        c.type = static_cast<domain::sketch::DimensionalConstraintType>(require<int>(j, "type"));
        c.refs = j.value("refs", std::vector<EntityRefDto>{});
        c.value = require<double>(j, "value");
        c.driving = j.value("driving", true);
        c.units = j.value("units", std::string("mm"));
    }

    // --- Sketch/Document/File ---
    void to_json(json& j, const SketchDto& s) {
        j = json{ {"id", s.id}, {"name", s.name}, {"visible", s.visible} };
        j["entities"] = json::array();
        for (const auto& e : s.entities) j["entities"].push_back(entityToJson(e));
        j["constraints"] = json::array();
        for (const auto& c : s.constraints) j["constraints"].push_back(constraintToJson(c));
    }
    void from_json(const json& j, SketchDto& s) {
        s.id = require<domain::sketch::SketchId>(j, "id");
        s.name = j.value("name", "");
        s.visible = j.value("visible", true);

        s.entities.clear();
        for (const auto& je : j.value("entities", json::array())) {
            s.entities.emplace_back(entityFromJson(je));
        }

        s.constraints.clear();
        for (const auto& jc : j.value("constraints", json::array())) {
            s.constraints.emplace_back(constraintFromJson(jc));
        }
    }

    void to_json(json& j, const DocumentDto& d) {
        j = json{ {"id", d.id}, {"name", d.name}, {"sketches", d.sketches} };
    }
    void from_json(const json& j, DocumentDto& d) {
        d.id = require<domain::sketch::DocumentId>(j, "id");
        d.name = j.value("name", "");
        d.sketches = j.value("sketches", std::vector<SketchDto>{});
    }

    void to_json(json& j, const FileDto& f) {
        j = json{ {"fileVersion", f.fileVersion}, {"document", f.document} };
    }
    void from_json(const json& j, FileDto& f) {
        f.fileVersion = j.value("fileVersion", kSketchFileVersion);
        f.document = require<DocumentDto>(j, "document");
    }

    // --- Tagged variants ---
    static const char* entityKindToString(const EntityDto& e) {
        if (std::holds_alternative<PointDto>(e)) return "Point";
        if (std::holds_alternative<LineDto>(e)) return "Line";
        if (std::holds_alternative<CircleDto>(e)) return "Circle";
        if (std::holds_alternative<ArcDto>(e)) return "Arc";
        if (std::holds_alternative<EllipseDto>(e)) return "Ellipse";
        if (std::holds_alternative<CurveDto>(e)) return "Curve";
        return "Unknown";
    }

    json entityToJson(const EntityDto& e) {
        json j;
        j["kind"] = entityKindToString(e);
        json payload;
        std::visit([&](auto&& v) { payload = v; }, e);
        j["data"] = payload;
        return j;
    }

    EntityDto entityFromJson(const json& j) {
        const std::string kind = require<std::string>(j, "kind");
        const json data = require<json>(j, "data");

        if (kind == "Point") return data.get<PointDto>();
        if (kind == "Line") return data.get<LineDto>();
        if (kind == "Circle") return data.get<CircleDto>();
        if (kind == "Arc") return data.get<ArcDto>();
        if (kind == "Ellipse") return data.get<EllipseDto>();
        if (kind == "Curve") return data.get<CurveDto>();
        throw std::runtime_error("Unknown entity kind: " + kind);
    }

    json constraintToJson(const ConstraintDto& c) {
        json j;
        if (std::holds_alternative<GeometricConstraintDto>(c)) {
            j["kind"] = "Geometric";
            j["data"] = std::get<GeometricConstraintDto>(c);
        } else {
            j["kind"] = "Dimensional";
            j["data"] = std::get<DimensionalConstraintDto>(c);
        }
        return j;
    }

    ConstraintDto constraintFromJson(const json& j) {
        const std::string kind = require<std::string>(j, "kind");
        const json data = require<json>(j, "data");
        if (kind == "Geometric") return data.get<GeometricConstraintDto>();
        if (kind == "Dimensional") return data.get<DimensionalConstraintDto>();
        throw std::runtime_error("Unknown constraint kind: " + kind);
    }

}

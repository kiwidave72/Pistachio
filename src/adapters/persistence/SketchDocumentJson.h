#pragma once

// JSON persistence uses nlohmann::json.
// If you don't already have it in your build, install via vcpkg: nlohmann-json
// and ensure your include paths contain <nlohmann/json.hpp>.

#include <nlohmann/json.hpp>

#include "SketchDocumentDto.h"

namespace domain::sketch {
    // ADL-friendly JSON conversions for domain value types
    void to_json(nlohmann::json& j, const Vec2& v);
    void from_json(const nlohmann::json& j, Vec2& v);
}

namespace adapters::persistence::dto {

    using json = nlohmann::json;

    // ADL-friendly JSON conversions for DTOs (must live in the DTO namespace)
    void to_json(json& j, const EntityRefDto& r);
    void from_json(const json& j, EntityRefDto& r);

    void to_json(json& j, const ConstraintMetaDto& m);
    void from_json(const json& j, ConstraintMetaDto& m);

    void to_json(json& j, const PointDto& e);
    void from_json(const json& j, PointDto& e);

    void to_json(json& j, const LineDto& e);
    void from_json(const json& j, LineDto& e);

    void to_json(json& j, const CircleDto& e);
    void from_json(const json& j, CircleDto& e);

    void to_json(json& j, const ArcDto& e);
    void from_json(const json& j, ArcDto& e);

    void to_json(json& j, const EllipseDto& e);
    void from_json(const json& j, EllipseDto& e);

    void to_json(json& j, const CurveDto& e);
    void from_json(const json& j, CurveDto& e);

    void to_json(json& j, const GeometricConstraintDto& c);
    void from_json(const json& j, GeometricConstraintDto& c);

    void to_json(json& j, const DimensionalConstraintDto& c);
    void from_json(const json& j, DimensionalConstraintDto& c);

    void to_json(json& j, const SketchDto& s);
    void from_json(const json& j, SketchDto& s);

    void to_json(json& j, const DocumentDto& d);
    void from_json(const json& j, DocumentDto& d);

    void to_json(json& j, const FileDto& f);
    void from_json(const json& j, FileDto& f);

    // Variant helpers (tagged unions)
    json entityToJson(const EntityDto& e);
    EntityDto entityFromJson(const json& j);

    json constraintToJson(const ConstraintDto& c);
    ConstraintDto constraintFromJson(const json& j);

} // namespace adapters::persistence::dto

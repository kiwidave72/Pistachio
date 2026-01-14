#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "domain/SketchIds.h"
#include "domain/SketchMath.h"

#include "domain/SketchConstraints.h"


namespace adapters::persistence::dto {

    // File schema versioning. Increment when you introduce breaking changes.
    inline constexpr int kSketchFileVersion = 1;

    // ----- Entity DTOs -----
    struct PointDto {
        domain::sketch::EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
        domain::sketch::Vec2 p;
    };

    struct LineDto {
        domain::sketch::EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
        domain::sketch::Vec2 a;
        domain::sketch::Vec2 b;
    };

    struct CircleDto {
        domain::sketch::EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
        domain::sketch::Vec2 center;
        double radius{ 1.0 };
    };

    struct ArcDto {
        domain::sketch::EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
        domain::sketch::Vec2 center;
        double radius{ 1.0 };
        domain::sketch::Vec2 start;
        domain::sketch::Vec2 end;
        bool ccw{ true };
    };

    struct EllipseDto {
        domain::sketch::EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
        domain::sketch::Vec2 center;
        double rx{ 2.0 };
        double ry{ 1.0 };
        double rotation{ 0.0 };
    };

    struct CurveDto {
        domain::sketch::EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
        std::vector<domain::sketch::Vec2> controlPoints;
        bool closed{ false };
    };

    using EntityDto = std::variant<PointDto, LineDto, CircleDto, ArcDto, EllipseDto, CurveDto>;

    // ----- Constraint DTOs -----

    struct EntityRefDto {
        domain::sketch::EntityId id{};
        domain::sketch::EntityAnchor anchor{ domain::sketch::EntityAnchor::None };
    };

    struct ConstraintMetaDto {
        domain::sketch::ConstraintId id{};
        std::string name;
        bool enabled{ true };
        bool suppressed{ false };
    };

    struct GeometricConstraintDto {
        ConstraintMetaDto meta;
        domain::sketch::GeometricConstraintType type{};
        std::vector<EntityRefDto> refs;
        std::optional<double> param;
    };

    struct DimensionalConstraintDto {
        ConstraintMetaDto meta;
        domain::sketch::DimensionalConstraintType type{};
        std::vector<EntityRefDto> refs;
        double value{ 0.0 };
        bool driving{ true };
        std::string units{ "mm" };
    };

    using ConstraintDto = std::variant<GeometricConstraintDto, DimensionalConstraintDto>;

    // ----- Sketch + Document DTOs -----

    struct SketchDto {
        domain::sketch::SketchId id{};
        std::string name;
        bool visible{ true };

        std::vector<EntityDto> entities;
        std::vector<ConstraintDto> constraints;
    };

    struct DocumentDto {
        domain::sketch::DocumentId id{};
        std::string name;
        std::vector<SketchDto> sketches;
    };

    struct FileDto {
        int fileVersion{ kSketchFileVersion };
        DocumentDto document;
    };

} // namespace adapters::persistence::dto

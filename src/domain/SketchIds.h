#pragma once

#include <cstdint>

namespace domain::sketch {

    using DocumentId = std::uint64_t;
    using SketchId = std::uint64_t;
    using EntityId = std::uint64_t;
    using ConstraintId = std::uint64_t;

    enum class EntityKind : std::uint8_t {
        Point,
        Line,
        Circle,
        Arc,
        Ellipse,
        Curve
    };

    // Fine-grained location on an entity for constraints/dimensions.
    enum class EntityAnchor : std::uint8_t {
        None = 0,

        // Point
        Point,

        // Line
        LineStart,
        LineEnd,
        LineMid,
        LineInfinite,

        // Circle / Arc / Ellipse
        Center,
        RadiusPoint,
        Start,
        End,

        // Curve (spline/polyline)
        CurvePoint0,
        CurvePoint1,
        CurveTangent0,
        CurveTangent1,

        AnyPoint
    };

    struct EntityRef {
        EntityId id{};
        EntityAnchor anchor{ EntityAnchor::None };
    };

} // namespace domain::sketch

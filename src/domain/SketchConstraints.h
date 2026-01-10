#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "SketchIds.h"

namespace domain::sketch {

    enum class GeometricConstraintType : std::uint8_t {
        Horizontal,
        Vertical,
        Coincident,
        Tangent,
        Perpendicular,
        Parallel,
        Collinear,
        Midpoint,
        Concentric,
        Symmetry,
        Fix,
        Curvature
    };

    enum class DimensionalConstraintType : std::uint8_t {
        Distance,
        Length,
        Angle,
        Radius,
        Diameter
    };

    struct ConstraintMeta {
        ConstraintId id{};
        std::string name;
        bool enabled{ true };
        bool suppressed{ false };
    };

    struct GeometricConstraint {
        ConstraintMeta meta{};
        GeometricConstraintType type{};
        std::vector<EntityRef> refs;
        std::optional<double> param; // optional numeric parameter
    };

    struct DimensionalConstraint {
        ConstraintMeta meta{};
        DimensionalConstraintType type{};
        std::vector<EntityRef> refs;
        double value{ 0.0 };
        bool driving{ true };      // true=fixed (driving), false=driven
        std::string units{ "mm" };
    };

    using Constraint = std::variant<GeometricConstraint, DimensionalConstraint>;

} // namespace domain::sketch

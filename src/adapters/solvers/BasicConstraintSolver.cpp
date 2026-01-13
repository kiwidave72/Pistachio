#include "BasicConstraintSolver.h"
 
#include "adapters/solvers/BasicConstraintSolver.h"
#include "domain/SketchModel.h"   
#include <algorithm>
#include <cmath>
#include <variant>


using namespace domain::sketch;

namespace adapters {

    double dist2(const Vec2& a, const Vec2& b) {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        return dx * dx + dy * dy;
    }

    static domain::sketch::Vec2* resolveAnchor(domain::sketch::EntityStore& store,
        const domain::sketch::EntityRef& r)
    {
        using namespace domain::sketch;

        if (!store.contains(r.id))
            return nullptr;

        const EntityHandle h = store.getHandle(r.id);

        switch (h.kind)
        {
        case EntityKind::Point:
        {
            auto& p = store.point(h.index);
            return &p.p; // anchor ignored, point is a point
        }

        case EntityKind::Line:
        {
            auto& l = store.line(h.index);
            if (r.anchor == EntityAnchor::LineStart) return &l.a;
            if (r.anchor == EntityAnchor::LineEnd)   return &l.b;

            // Optional: support "AnyPoint" for convenience
            if (r.anchor == EntityAnchor::AnyPoint || r.anchor == EntityAnchor::None)
                return &l.a;

            return nullptr;
        }

        case EntityKind::Circle:
        {
            auto& c = store.circle(h.index);
            if (r.anchor == EntityAnchor::Center) return &c.center;

            // Optional: RadiusPoint isn't explicitly stored in Circle2D, so we can't return a Vec2*
            // If you want RadiusPoint support later, you'd compute it or represent it as a derived anchor.
            return nullptr;
        }

        case EntityKind::Arc:
        {
            auto& a = store.arc(h.index);
            if (r.anchor == EntityAnchor::Center) return &a.center;
            if (r.anchor == EntityAnchor::Start)  return &a.start; // <-- FIX (not ArcStart)
            if (r.anchor == EntityAnchor::End)    return &a.end;   // <-- FIX (not ArcEnd)
            return nullptr;
        }

        case EntityKind::Ellipse:
        {
            auto& e = store.ellipse(h.index);
            if (r.anchor == EntityAnchor::Center) return &e.center;
            return nullptr;
        }

        case EntityKind::Curve:
        {
            // Curve points are in a vector => returning a stable Vec2* is only safe if controlPoints won't reallocate.
            // For Phase 2, simplest: don't support curve anchors yet.
            return nullptr;
        }

        default:
            return nullptr;
        }
    }


    bool applyCoincident(EntityStore& store, const GeometricConstraint& c, double& maxDelta) {
        if (c.refs.size() < 2) return false;

        Vec2* a = resolveAnchor(store, c.refs[0]);
        Vec2* b = resolveAnchor(store, c.refs[1]);
        if (!a || !b) return false;

        const Vec2 oldB = *b;
        *b = *a;

        const double d = std::sqrt(dist2(oldB, *b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

    bool applyHorizontal(domain::sketch::EntityStore& store,
        const domain::sketch::GeometricConstraint& c,
        double& maxDelta)
    {
        using namespace domain::sketch;

        Vec2* a = nullptr;
        Vec2* b = nullptr;

        // Legacy format: one ref that points at the line
        if (c.refs.size() == 1) {
            const auto& r = c.refs[0];

            if (!store.contains(r.id)) return false;
            auto h = store.getHandle(r.id);
            if (h.kind != EntityKind::Line) return false;

            auto& ln = store.line(h.index);
            a = &ln.a;
            b = &ln.b;
        }
        // New format: two anchors
        else if (c.refs.size() >= 2) {
            a = resolveAnchor(store, c.refs[0]);
            b = resolveAnchor(store, c.refs[1]);
            if (!a || !b) return false;
        }
        else {
            return false;
        }

        const Vec2 oldB = *b;
        b->y = a->y;

        const double dx = oldB.x - b->x;
        const double dy = oldB.y - b->y;
        const double d = std::sqrt(dx * dx + dy * dy);
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }


    bool applyVertical(EntityStore& store, const GeometricConstraint& c, double& maxDelta) {
        if (c.refs.size() < 2) return false;

        Vec2* a = resolveAnchor(store, c.refs[0]);
        Vec2* b = resolveAnchor(store, c.refs[1]);
        if (!a || !b) return false;

        const Vec2 oldB = *b;
        b->x = a->x;

        const double d = std::sqrt(dist2(oldB, *b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

    bool applyGeometric(EntityStore& store, const GeometricConstraint& gc, double& maxDelta) {
        if (!gc.meta.enabled || gc.meta.suppressed) return false;

        switch (gc.type) {
        case GeometricConstraintType::Coincident: return applyCoincident(store, gc, maxDelta);
        case GeometricConstraintType::Horizontal: return applyHorizontal(store, gc, maxDelta);
        case GeometricConstraintType::Vertical:   return applyVertical(store, gc, maxDelta);
        default: return false; // Phase 2 = only these three for now
        }
    }

}

namespace adapters::solver {

    ports::ResolvedSketch BasicConstraintSolver::solve(const Sketch& input) {
        ports::ResolvedSketch out;
        out.sketch = input; // Phase 1: start from a copy

        constexpr int kMaxIterations = 32;
        constexpr double kEpsilon = 1e-6;

        bool anyChange = false;
        double maxDelta = 0.0;

        for (int it = 0; it < kMaxIterations; ++it) {
            bool changedThisIter = false;
            double iterMaxDelta = 0.0;

            for (const auto& cVar : out.sketch.constraints) {
                if (auto gc = std::get_if<GeometricConstraint>(&cVar)) {
                    changedThisIter |= applyGeometric(out.sketch.entities, *gc, iterMaxDelta);
                }
            }

            anyChange |= changedThisIter;
            maxDelta = std::max(maxDelta, iterMaxDelta);

            if (!changedThisIter || iterMaxDelta < kEpsilon) {
                out.report.converged = true;
                out.report.iterations = it + 1;
                out.report.maxDelta = maxDelta;
                return out;
            }
        }

        out.report.converged = false;
        out.report.iterations = kMaxIterations;
        out.report.maxDelta = maxDelta;
        if (anyChange) out.report.warnings.push_back("Solver hit max iterations (may be conflicting constraints).");
        return out;
    }

}

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

// --- Tangent constraint (Phase 2 minimal) ---
// Supports:
//   - Line <-> Circle: move the circle center so distance(center, line) == radius
//   - Circle <-> Circle: move the second circle so center distance == r1 + r2 (external tangent)
static bool applyTangent(EntityStore& store, const GeometricConstraint& c, double& maxDelta)
{
    if (c.refs.size() < 2) return false;
    if (!store.contains(c.refs[0].id) || !store.contains(c.refs[1].id)) return false;

    const EntityHandle ha = store.getHandle(c.refs[0].id);
    const EntityHandle hb = store.getHandle(c.refs[1].id);

    auto tangentLineCircle = [&](const EntityHandle& hLine, const EntityHandle& hCircle) -> bool
    {
        if (hLine.kind != EntityKind::Line || hCircle.kind != EntityKind::Circle) return false;

        auto& ln = store.line(hLine.index);
        auto& cc = store.circle(hCircle.index);

        const Vec2 a = ln.a;
        const Vec2 b = ln.b;

        const double vx = b.x - a.x;
        const double vy = b.y - a.y;
        const double len = std::sqrt(vx * vx + vy * vy);
        if (len < 1e-12) return false;

        // Signed distance from point to infinite line (a->b)
        const double nx = -vy / len; // unit normal
        const double ny =  vx / len;

        const double px = cc.center.x - a.x;
        const double py = cc.center.y - a.y;

        const double signedD = px * nx + py * ny;         // positive on one side, negative on the other
        const double absD = std::abs(signedD);

        const double err = cc.radius - absD;
        if (std::abs(err) < 1e-9) return false;

        // Preserve the current side of the line (sign of signedD).
        const double side = (signedD >= 0.0) ? 1.0 : -1.0;

        Vec2 old = cc.center;
        cc.center.x += nx * side * err;
        cc.center.y += ny * side * err;

        const double d = std::sqrt(dist2(old, cc.center));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    };

    auto tangentCircleCircle = [&](const EntityHandle& hC1, const EntityHandle& hC2) -> bool
    {
        if (hC1.kind != EntityKind::Circle || hC2.kind != EntityKind::Circle) return false;

        auto& c1 = store.circle(hC1.index);
        auto& c2 = store.circle(hC2.index);

        const double dx = c2.center.x - c1.center.x;
        const double dy = c2.center.y - c1.center.y;
        const double d = std::sqrt(dx * dx + dy * dy);
        if (d < 1e-12) return false;

        const double target = c1.radius + c2.radius; // external tangency
        const double err = target - d;
        if (std::abs(err) < 1e-9) return false;

        const double ux = dx / d;
        const double uy = dy / d;

        Vec2 old = c2.center;
        c2.center.x += ux * err;
        c2.center.y += uy * err;

        const double moved = std::sqrt(dist2(old, c2.center));
        maxDelta = std::max(maxDelta, moved);
        return moved > 0.0;
    };

    // Try both orderings (user can pick in any order)
    if (tangentLineCircle(ha, hb)) return true;
    if (tangentLineCircle(hb, ha)) return true;
    if (tangentCircleCircle(ha, hb)) return true;
    if (tangentCircleCircle(hb, ha)) return true;

    return false;
}



    bool applyGeometric(EntityStore& store, const GeometricConstraint& gc, double& maxDelta) {
        if (!gc.meta.enabled || gc.meta.suppressed) return false;

        switch (gc.type) {
        case GeometricConstraintType::Coincident: return applyCoincident(store, gc, maxDelta);
        case GeometricConstraintType::Horizontal: return applyHorizontal(store, gc, maxDelta);
        case GeometricConstraintType::Vertical:   return applyVertical(store, gc, maxDelta);
        case GeometricConstraintType::Tangent:    return applyTangent(store, gc, maxDelta);
        default: return false; // Phase 2 = minimal set implemented
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

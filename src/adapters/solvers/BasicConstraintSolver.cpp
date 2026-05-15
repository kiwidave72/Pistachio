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

    // Result of closest point on segment calculation
    struct SegmentPointResult {
        Vec2 closestPoint;
        double distance;
        bool isOnSegment;  // true if closest point is between a and b (not beyond endpoints)
    };

    /**
     * Compute the closest point on line segment [a, b] to point p.
     * 
     * This is different from distance to an INFINITE line:
     * - If p projects onto the segment, return the projection
     * - If p projects beyond 'a', return 'a'
     * - If p projects beyond 'b', return 'b'
     * 
     * Returns:
     *   - closestPoint: the actual closest point on the segment
     *   - distance: Euclidean distance from p to closestPoint
     *   - isOnSegment: true if closest point is strictly between a and b (not at endpoints)
     */
    static SegmentPointResult closestPointOnSegment(
        const Vec2& p,  // test point
        const Vec2& a,  // segment start
        const Vec2& b   // segment end
    ) {
        SegmentPointResult result;
        
        // Vector from a to b
        const double vx = b.x - a.x;
        const double vy = b.y - a.y;
        const double segmentLengthSq = vx * vx + vy * vy;
        
        // Degenerate segment (a == b)
        if (segmentLengthSq < 1e-12) {
            result.closestPoint = a;
            result.distance = std::sqrt(dist2(p, a));
            result.isOnSegment = false;
            return result;
        }
        
        // Vector from a to p
        const double px = p.x - a.x;
        const double py = p.y - a.y;
        
        // Project p onto the line defined by a->b
        // t = (p - a) · (b - a) / |b - a|²
        const double t = (px * vx + py * vy) / segmentLengthSq;
        
        // Clamp t to [0, 1] to stay on the segment
        if (t <= 0.0) {
            // Closest point is 'a'
            result.closestPoint = a;
            result.distance = std::sqrt(dist2(p, a));
            result.isOnSegment = false;
        }
        else if (t >= 1.0) {
            // Closest point is 'b'
            result.closestPoint = b;
            result.distance = std::sqrt(dist2(p, b));
            result.isOnSegment = false;
        }
        else {
            // Closest point is on the segment
            result.closestPoint.x = a.x + t * vx;
            result.closestPoint.y = a.y + t * vy;
            result.distance = std::sqrt(dist2(p, result.closestPoint));
            result.isOnSegment = true;
        }
        
        return result;
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
            if (r.anchor == EntityAnchor::Start)  return &a.start;
            if (r.anchor == EntityAnchor::End)    return &a.end;
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

    // --- Horizontal Constraint (IMPROVED) ---
    // Makes a line horizontal OR makes two points have the same Y coordinate
    // Supports:
    //   - 1 ref: Line entity (makes line horizontal)
    //   - 2 refs: Two points or line endpoints (aligns Y coordinates)
    bool applyHorizontal(domain::sketch::EntityStore& store,
        const domain::sketch::GeometricConstraint& c,
        double& maxDelta)
    {
        using namespace domain::sketch;

        Vec2* a = nullptr;
        Vec2* b = nullptr;

        // Format 1: One ref pointing to a line entity
        if (c.refs.size() == 1) {
            const auto& r = c.refs[0];

            if (!store.contains(r.id)) return false;
            auto h = store.getHandle(r.id);
            if (h.kind != EntityKind::Line) return false;

            auto& ln = store.line(h.index);
            a = &ln.a;
            b = &ln.b;
        }
        // Format 2: Two refs (points or line endpoints)
        else if (c.refs.size() >= 2) {
            a = resolveAnchor(store, c.refs[0]);
            b = resolveAnchor(store, c.refs[1]);
            if (!a || !b) return false;
        }
        else {
            return false;
        }

        // Make horizontal: align Y coordinates
        // Move the second point to match the first point's Y
        const Vec2 oldB = *b;
        b->y = a->y;

        const double d = std::sqrt(dist2(oldB, *b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

    // --- Vertical Constraint (IMPROVED) ---
    // Makes a line vertical OR makes two points have the same X coordinate
    // Supports:
    //   - 1 ref: Line entity (makes line vertical)
    //   - 2 refs: Two points or line endpoints (aligns X coordinates)
    bool applyVertical(EntityStore& store, const GeometricConstraint& c, double& maxDelta) {
        using namespace domain::sketch;

        Vec2* a = nullptr;
        Vec2* b = nullptr;

        // Format 1: One ref pointing to a line entity
        if (c.refs.size() == 1) {
            const auto& r = c.refs[0];

            if (!store.contains(r.id)) return false;
            auto h = store.getHandle(r.id);
            if (h.kind != EntityKind::Line) return false;

            auto& ln = store.line(h.index);
            a = &ln.a;
            b = &ln.b;
        }
        // Format 2: Two refs (points or line endpoints)
        else if (c.refs.size() >= 2) {
            a = resolveAnchor(store, c.refs[0]);
            b = resolveAnchor(store, c.refs[1]);
            if (!a || !b) return false;
        }
        else {
            return false;
        }

        // Make vertical: align X coordinates
        // Move the second point to match the first point's X
        const Vec2 oldB = *b;
        b->x = a->x;

        const double d = std::sqrt(dist2(oldB, *b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

    // --- Parallel Constraint (NEW) ---
    // Makes two lines parallel by rotating the second line to match the first line's direction
    // Requires: 2 refs, both must be Line entities
    static bool applyParallel(EntityStore& store, const GeometricConstraint& c, double& maxDelta) {
        using namespace domain::sketch;

        if (c.refs.size() < 2) return false;
        if (!store.contains(c.refs[0].id) || !store.contains(c.refs[1].id)) return false;

        const EntityHandle h1 = store.getHandle(c.refs[0].id);
        const EntityHandle h2 = store.getHandle(c.refs[1].id);

        if (h1.kind != EntityKind::Line || h2.kind != EntityKind::Line) return false;

        auto& line1 = store.line(h1.index);
        auto& line2 = store.line(h2.index);

        // Direction of first line (reference)
        const double dx1 = line1.b.x - line1.a.x;
        const double dy1 = line1.b.y - line1.a.y;
        const double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

        if (len1 < 1e-12) return false; // Degenerate line

        // Unit direction vector of line1
        const double ux1 = dx1 / len1;
        const double uy1 = dy1 / len1;

        // Current direction of second line
        const double dx2 = line2.b.x - line2.a.x;
        const double dy2 = line2.b.y - line2.a.y;
        const double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

        if (len2 < 1e-12) return false; // Degenerate line

        // Check if already parallel (dot product of unit vectors ≈ ±1)
        const double ux2 = dx2 / len2;
        const double uy2 = dy2 / len2;
        const double dot = ux1 * ux2 + uy1 * uy2;

        if (std::abs(std::abs(dot) - 1.0) < 1e-9) {
            return false; // Already parallel
        }

        // Make line2 parallel to line1 by adjusting line2.b
        // Keep line2.a fixed, move line2.b to maintain length but match direction
        Vec2 oldB = line2.b;

        // New direction: same as line1 (or opposite if currently opposing)
        // Use dot product to determine if we should flip direction
        const double sign = (dot >= 0.0) ? 1.0 : -1.0;

        line2.b.x = line2.a.x + sign * ux1 * len2;
        line2.b.y = line2.a.y + sign * uy1 * len2;

        const double d = std::sqrt(dist2(oldB, line2.b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

    // --- Perpendicular Constraint (NEW) ---
    // Makes two lines perpendicular by rotating the second line 90° to the first
    // Requires: 2 refs, both must be Line entities
    static bool applyPerpendicular(EntityStore& store, const GeometricConstraint& c, double& maxDelta) {
        using namespace domain::sketch;

        if (c.refs.size() < 2) return false;
        if (!store.contains(c.refs[0].id) || !store.contains(c.refs[1].id)) return false;

        const EntityHandle h1 = store.getHandle(c.refs[0].id);
        const EntityHandle h2 = store.getHandle(c.refs[1].id);

        if (h1.kind != EntityKind::Line || h2.kind != EntityKind::Line) return false;

        auto& line1 = store.line(h1.index);
        auto& line2 = store.line(h2.index);

        // Direction of first line (reference)
        const double dx1 = line1.b.x - line1.a.x;
        const double dy1 = line1.b.y - line1.a.y;
        const double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

        if (len1 < 1e-12) return false; // Degenerate line

        // Unit direction vector of line1
        const double ux1 = dx1 / len1;
        const double uy1 = dy1 / len1;

        // Perpendicular direction (rotate 90° counterclockwise)
        const double px = -uy1;
        const double py = ux1;

        // Current direction of second line
        const double dx2 = line2.b.x - line2.a.x;
        const double dy2 = line2.b.y - line2.a.y;
        const double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

        if (len2 < 1e-12) return false; // Degenerate line

        // Unit direction of line2
        const double ux2 = dx2 / len2;
        const double uy2 = dy2 / len2;

        // Check if already perpendicular (dot product ≈ 0)
        const double dot = ux1 * ux2 + uy1 * uy2;

        if (std::abs(dot) < 1e-9) {
            return false; // Already perpendicular
        }

        // Make line2 perpendicular to line1
        // Keep line2.a fixed, move line2.b
        Vec2 oldB = line2.b;

        // Determine which perpendicular direction to use (choose closest to current)
        const double dot_perp1 = px * ux2 + py * uy2;  // dot with +90° rotation
        const double dot_perp2 = -px * ux2 - py * uy2; // dot with -90° rotation

        if (std::abs(dot_perp1) > std::abs(dot_perp2)) {
            // Use +90° rotation
            line2.b.x = line2.a.x + px * len2;
            line2.b.y = line2.a.y + py * len2;
        } else {
            // Use -90° rotation
            line2.b.x = line2.a.x - px * len2;
            line2.b.y = line2.a.y - py * len2;
        }

        const double d = std::sqrt(dist2(oldB, line2.b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

    // --- Equal Length Constraint (NEW) ---
    // Makes two lines have the same length
    // This is a dimensional constraint that adjusts line2.b to match line1's length
    // Requires: 2 refs, both must be Line entities
    static bool applyEqualLength(EntityStore& store, const GeometricConstraint& c, double& maxDelta) {
        using namespace domain::sketch;

        if (c.refs.size() < 2) return false;
        if (!store.contains(c.refs[0].id) || !store.contains(c.refs[1].id)) return false;

        const EntityHandle h1 = store.getHandle(c.refs[0].id);
        const EntityHandle h2 = store.getHandle(c.refs[1].id);

        if (h1.kind != EntityKind::Line || h2.kind != EntityKind::Line) return false;

        auto& line1 = store.line(h1.index);
        auto& line2 = store.line(h2.index);

        // Length of first line (reference)
        const double dx1 = line1.b.x - line1.a.x;
        const double dy1 = line1.b.y - line1.a.y;
        const double len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

        // Current length of second line
        const double dx2 = line2.b.x - line2.a.x;
        const double dy2 = line2.b.y - line2.a.y;
        const double len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

        if (len2 < 1e-12) return false; // Degenerate line

        // Check if already equal length
        if (std::abs(len1 - len2) < 1e-9) {
            return false; // Already equal
        }

        // Adjust line2.b to match line1's length
        // Keep line2.a and direction fixed, scale to new length
        const double ux2 = dx2 / len2;
        const double uy2 = dy2 / len2;

        Vec2 oldB = line2.b;
        line2.b.x = line2.a.x + ux2 * len1;
        line2.b.y = line2.a.y + uy2 * len1;

        const double d = std::sqrt(dist2(oldB, line2.b));
        maxDelta = std::max(maxDelta, d);
        return d > 0.0;
    }

// --- Tangent constraint (FINITE SEGMENT VERSION) ---
// Supports:
//   - Line <-> Circle: move the circle center so distance(center, segment) == radius
//                      Only applies if tangent point is ON the segment (not beyond endpoints)
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

        // Compute closest point on the LINE SEGMENT to the circle center
        SegmentPointResult sp = closestPointOnSegment(cc.center, ln.a, ln.b);
        
        // CRITICAL: Only apply tangent if the tangent point would be ON the segment
        // This prevents the circle from being "tangent" to the infinite extension of the line
        // 
        // Exception: If the circle is very close to an endpoint, we allow endpoint tangency
        const double endpointTolerance = 1e-6;
        const bool nearEndpoint = 
            (std::abs(sp.distance - cc.radius) < endpointTolerance) && !sp.isOnSegment;
        
        if (!sp.isOnSegment && !nearEndpoint) {
            // The tangent point would be beyond the segment endpoints
            // Do not apply this constraint
            return false;
        }

        // Current distance from center to segment
        const double currentDist = sp.distance;
        const double err = cc.radius - currentDist;
        
        if (std::abs(err) < 1e-9) {
            // Already tangent
            return false;
        }

        // Move circle center toward/away from closest point to achieve tangency
        const double dx = cc.center.x - sp.closestPoint.x;
        const double dy = cc.center.y - sp.closestPoint.y;
        const double d = std::sqrt(dx * dx + dy * dy);
        
        if (d < 1e-12) {
            // Circle center is exactly on the segment - degenerate case
            // Move perpendicular to the segment
            const double vx = ln.b.x - ln.a.x;
            const double vy = ln.b.y - ln.a.y;
            const double len = std::sqrt(vx * vx + vy * vy);
            if (len < 1e-12) return false;
            
            const double nx = -vy / len;
            const double ny = vx / len;
            
            Vec2 old = cc.center;
            cc.center.x += nx * cc.radius;
            cc.center.y += ny * cc.radius;
            
            const double moved = std::sqrt(dist2(old, cc.center));
            maxDelta = std::max(maxDelta, moved);
            return moved > 0.0;
        }

        // Normal case: move along the direction from closestPoint to center
        const double ux = dx / d;
        const double uy = dy / d;

        Vec2 old = cc.center;
        cc.center.x = sp.closestPoint.x + ux * cc.radius;
        cc.center.y = sp.closestPoint.y + uy * cc.radius;

        const double moved = std::sqrt(dist2(old, cc.center));
        maxDelta = std::max(maxDelta, moved);
        return moved > 0.0;
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
        case GeometricConstraintType::Coincident:     return applyCoincident(store, gc, maxDelta);
        case GeometricConstraintType::Horizontal:     return applyHorizontal(store, gc, maxDelta);
        case GeometricConstraintType::Vertical:       return applyVertical(store, gc, maxDelta);
        case GeometricConstraintType::Parallel:       return applyParallel(store, gc, maxDelta);
        case GeometricConstraintType::Perpendicular:  return applyPerpendicular(store, gc, maxDelta);
        case GeometricConstraintType::Tangent:        return applyTangent(store, gc, maxDelta);
        // Note: Equal length uses a geometric constraint type in some systems
        // If your system has a separate Equal type, add it here
        default: return false;
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

// Unit tests for finite segment tangent constraint
// Add to tests/test_constraints.cpp or create new test file

#include "mini_test.h"
#include "adapters/solvers/BasicConstraintSolver.h"
#include "domain/SketchModel.h"
#include <cmath>

using namespace domain::sketch;

namespace {

// Helper to compare doubles with tolerance
bool near(double a, double b, double tol = 1e-9) {
    return std::abs(a - b) < tol;
}

// Helper to compare Vec2
bool near(const Vec2& a, const Vec2& b, double tol = 1e-9) {
    return near(a.x, b.x, tol) && near(a.y, b.y, tol);
}

} // anonymous namespace


TEST(SegmentPointDistance, PointProjectsOntoMiddle) {
    // Segment from (0, 0) to (10, 0)
    // Point at (5, 3) should project to (5, 0), distance = 3, isOnSegment = true
    
    Vec2 a{0, 0};
    Vec2 b{10, 0};
    Vec2 p{5, 3};
    
    auto result = closestPointOnSegment(p, a, b);
    
    ASSERT(near(result.closestPoint, Vec2{5, 0}));
    ASSERT(near(result.distance, 3.0));
    ASSERT(result.isOnSegment);
}

TEST(SegmentPointDistance, PointProjectsBeyondStart) {
    // Segment from (0, 0) to (10, 0)
    // Point at (-2, 3) projects beyond start, should return (0, 0)
    
    Vec2 a{0, 0};
    Vec2 b{10, 0};
    Vec2 p{-2, 3};
    
    auto result = closestPointOnSegment(p, a, b);
    
    ASSERT(near(result.closestPoint, Vec2{0, 0}));
    ASSERT(near(result.distance, std::sqrt(4 + 9)));
    ASSERT(!result.isOnSegment);
}

TEST(SegmentPointDistance, PointProjectsBeyondEnd) {
    // Segment from (0, 0) to (10, 0)
    // Point at (12, 3) projects beyond end, should return (10, 0)
    
    Vec2 a{0, 0};
    Vec2 b{10, 0};
    Vec2 p{12, 3};
    
    auto result = closestPointOnSegment(p, a, b);
    
    ASSERT(near(result.closestPoint, Vec2{10, 0}));
    ASSERT(near(result.distance, std::sqrt(4 + 9)));
    ASSERT(!result.isOnSegment);
}

TEST(SegmentPointDistance, DegenerateSegment) {
    // Segment where a == b
    Vec2 a{5, 5};
    Vec2 b{5, 5};
    Vec2 p{8, 5};
    
    auto result = closestPointOnSegment(p, a, b);
    
    ASSERT(near(result.closestPoint, Vec2{5, 5}));
    ASSERT(near(result.distance, 3.0));
    ASSERT(!result.isOnSegment);
}

TEST(SegmentPointDistance, DiagonalSegment) {
    // Segment from (0, 0) to (4, 3)
    // Point at (2, 1.5) should be very close to the segment
    
    Vec2 a{0, 0};
    Vec2 b{4, 3};
    Vec2 p{2, 1.5};
    
    auto result = closestPointOnSegment(p, a, b);
    
    // Point is exactly on the segment
    ASSERT(near(result.closestPoint, Vec2{2, 1.5}));
    ASSERT(near(result.distance, 0.0));
    ASSERT(result.isOnSegment);
}


// --- Integration tests for tangent constraint ---

TEST(TangentConstraint, CircleTangentToSegmentMiddle) {
    // Create a horizontal line segment and a circle
    // Circle should be tangent to the middle of the segment
    
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Line from (0, 0) to (10, 0)
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{10, 0};
    auto lineHandle = sketch.entities.addLine(line);
    
    // Circle at (5, 5) with radius 2
    // Should move to (5, 2) to be tangent
    Circle2D circle;
    circle.h.id = sketch.nextEntityId++;
    circle.center = Vec2{5, 5};
    circle.radius = 2.0;
    auto circleHandle = sketch.entities.addCircle(circle);
    
    // Add tangent constraint
    GeometricConstraint tangent;
    tangent.meta.id = sketch.nextConstraintId++;
    tangent.type = GeometricConstraintType::Tangent;
    tangent.refs.push_back({line.h.id, EntityAnchor::None});
    tangent.refs.push_back({circle.h.id, EntityAnchor::Center});
    sketch.constraints.push_back(tangent);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // Check that circle moved to be tangent
    const auto& solvedCircle = result.sketch.entities.circle(circleHandle.index);
    ASSERT(near(solvedCircle.center.x, 5.0));  // x should stay at 5
    ASSERT(near(solvedCircle.center.y, 2.0));  // y should be radius away from line
}

TEST(TangentConstraint, CircleNotTangentBeyondSegment) {
    // Circle is positioned such that it would be tangent to the INFINITE line
    // but the tangent point is beyond the segment endpoint
    // Constraint should NOT apply
    
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Short line from (0, 0) to (2, 0)
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{2, 0};
    sketch.entities.addLine(line);
    
    // Circle at (10, 3) with radius 3
    // Tangent point would be at (10, 0) which is beyond segment end at (2, 0)
    Circle2D circle;
    circle.h.id = sketch.nextEntityId++;
    circle.center = Vec2{10, 3};
    circle.radius = 3.0;
    auto circleHandle = sketch.entities.addCircle(circle);
    
    Vec2 originalCenter = circle.center;
    
    // Add tangent constraint
    GeometricConstraint tangent;
    tangent.meta.id = sketch.nextConstraintId++;
    tangent.type = GeometricConstraintType::Tangent;
    tangent.refs.push_back({line.h.id, EntityAnchor::None});
    tangent.refs.push_back({circle.h.id, EntityAnchor::Center});
    sketch.constraints.push_back(tangent);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // Circle should NOT have moved (constraint didn't apply)
    const auto& solvedCircle = result.sketch.entities.circle(circleHandle.index);
    ASSERT(near(solvedCircle.center, originalCenter));
}

TEST(TangentConstraint, CircleTangentNearEndpoint) {
    // Circle tangent very close to endpoint should still work
    
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Line from (0, 0) to (10, 0)
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{10, 0};
    sketch.entities.addLine(line);
    
    // Circle at (9.5, 2) with radius 2 - tangent near endpoint
    Circle2D circle;
    circle.h.id = sketch.nextEntityId++;
    circle.center = Vec2{9.5, 3};
    circle.radius = 2.0;
    auto circleHandle = sketch.entities.addCircle(circle);
    
    // Add tangent constraint
    GeometricConstraint tangent;
    tangent.meta.id = sketch.nextConstraintId++;
    tangent.type = GeometricConstraintType::Tangent;
    tangent.refs.push_back({line.h.id, EntityAnchor::None});
    tangent.refs.push_back({circle.h.id, EntityAnchor::Center});
    sketch.constraints.push_back(tangent);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // Circle should have moved to be tangent
    const auto& solvedCircle = result.sketch.entities.circle(circleHandle.index);
    ASSERT(near(solvedCircle.center.x, 9.5, 0.1));  // x roughly preserved
    ASSERT(near(solvedCircle.center.y, 2.0, 0.1));  // y should be ~radius away
}

TEST(TangentConstraint, CircleCircleTangent) {
    // Two circles should be tangent (external)
    // This should still work as before
    
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Circle 1 at (0, 0) with radius 3
    Circle2D c1;
    c1.h.id = sketch.nextEntityId++;
    c1.center = Vec2{0, 0};
    c1.radius = 3.0;
    sketch.entities.addCircle(c1);
    
    // Circle 2 at (10, 0) with radius 2
    // Should move to (5, 0) to be tangent (distance = 3 + 2 = 5)
    Circle2D c2;
    c2.h.id = sketch.nextEntityId++;
    c2.center = Vec2{10, 0};
    c2.radius = 2.0;
    auto c2Handle = sketch.entities.addCircle(c2);
    
    // Add tangent constraint
    GeometricConstraint tangent;
    tangent.meta.id = sketch.nextConstraintId++;
    tangent.type = GeometricConstraintType::Tangent;
    tangent.refs.push_back({c1.h.id, EntityAnchor::Center});
    tangent.refs.push_back({c2.h.id, EntityAnchor::Center});
    sketch.constraints.push_back(tangent);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // Check distance between centers = r1 + r2
    const auto& sc1 = result.sketch.entities.circle(0);
    const auto& sc2 = result.sketch.entities.circle(c2Handle.index);
    
    double dx = sc2.center.x - sc1.center.x;
    double dy = sc2.center.y - sc1.center.y;
    double dist = std::sqrt(dx*dx + dy*dy);
    
    ASSERT(near(dist, 5.0));  // 3 + 2 = 5
}

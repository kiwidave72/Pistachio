// Comprehensive constraint tests for finite segment solver
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


// ============================================================================
// HORIZONTAL CONSTRAINT TESTS
// ============================================================================

TEST(HorizontalConstraint, SingleLineFormat) {
    // Test horizontal constraint with single ref (whole line)
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Diagonal line from (0, 0) to (10, 5)
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{10, 5};
    auto lineHandle = sketch.entities.addLine(line);
    
    // Add horizontal constraint (single ref format)
    GeometricConstraint horiz;
    horiz.meta.id = sketch.nextConstraintId++;
    horiz.type = GeometricConstraintType::Horizontal;
    horiz.refs.push_back({line.h.id, EntityAnchor::None});
    sketch.constraints.push_back(horiz);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solvedLine = result.sketch.entities.line(lineHandle.index);
    ASSERT(near(solvedLine.a.y, solvedLine.b.y)); // Y coordinates equal
}

TEST(HorizontalConstraint, TwoPointFormat) {
    // Test horizontal constraint with two refs (two endpoints)
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Diagonal line
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{10, 5};
    auto lineHandle = sketch.entities.addLine(line);
    
    // Add horizontal constraint (two ref format)
    GeometricConstraint horiz;
    horiz.meta.id = sketch.nextConstraintId++;
    horiz.type = GeometricConstraintType::Horizontal;
    horiz.refs.push_back({line.h.id, EntityAnchor::LineStart});
    horiz.refs.push_back({line.h.id, EntityAnchor::LineEnd});
    sketch.constraints.push_back(horiz);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solvedLine = result.sketch.entities.line(lineHandle.index);
    ASSERT(near(solvedLine.a.y, solvedLine.b.y)); // Y coordinates equal
    ASSERT(near(solvedLine.a.y, 0.0)); // First point Y stays at 0
}

TEST(HorizontalConstraint, TwoSeparatePoints) {
    // Align Y coordinates of two separate points
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    Point2D p1;
    p1.h.id = sketch.nextEntityId++;
    p1.p = Vec2{0, 0};
    auto p1Handle = sketch.entities.addPoint(p1);
    
    Point2D p2;
    p2.h.id = sketch.nextEntityId++;
    p2.p = Vec2{10, 5};
    auto p2Handle = sketch.entities.addPoint(p2);
    
    // Add horizontal constraint
    GeometricConstraint horiz;
    horiz.meta.id = sketch.nextConstraintId++;
    horiz.type = GeometricConstraintType::Horizontal;
    horiz.refs.push_back({p1.h.id, EntityAnchor::None});
    horiz.refs.push_back({p2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(horiz);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& sp1 = result.sketch.entities.point(p1Handle.index);
    const auto& sp2 = result.sketch.entities.point(p2Handle.index);
    ASSERT(near(sp1.p.y, sp2.p.y)); // Y coordinates equal
}


// ============================================================================
// VERTICAL CONSTRAINT TESTS
// ============================================================================

TEST(VerticalConstraint, SingleLineFormat) {
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Diagonal line from (0, 0) to (5, 10)
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{5, 10};
    auto lineHandle = sketch.entities.addLine(line);
    
    // Add vertical constraint
    GeometricConstraint vert;
    vert.meta.id = sketch.nextConstraintId++;
    vert.type = GeometricConstraintType::Vertical;
    vert.refs.push_back({line.h.id, EntityAnchor::None});
    sketch.constraints.push_back(vert);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solvedLine = result.sketch.entities.line(lineHandle.index);
    ASSERT(near(solvedLine.a.x, solvedLine.b.x)); // X coordinates equal
}

TEST(VerticalConstraint, TwoPointFormat) {
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Diagonal line
    Line2D line;
    line.h.id = sketch.nextEntityId++;
    line.a = Vec2{0, 0};
    line.b = Vec2{5, 10};
    auto lineHandle = sketch.entities.addLine(line);
    
    // Add vertical constraint (two ref format)
    GeometricConstraint vert;
    vert.meta.id = sketch.nextConstraintId++;
    vert.type = GeometricConstraintType::Vertical;
    vert.refs.push_back({line.h.id, EntityAnchor::LineStart});
    vert.refs.push_back({line.h.id, EntityAnchor::LineEnd});
    sketch.constraints.push_back(vert);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solvedLine = result.sketch.entities.line(lineHandle.index);
    ASSERT(near(solvedLine.a.x, solvedLine.b.x)); // X coordinates equal
    ASSERT(near(solvedLine.a.x, 0.0)); // First point X stays at 0
}


// ============================================================================
// PARALLEL CONSTRAINT TESTS
// ============================================================================

TEST(ParallelConstraint, TwoLines) {
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // First line: horizontal from (0, 0) to (10, 0)
    Line2D line1;
    line1.h.id = sketch.nextEntityId++;
    line1.a = Vec2{0, 0};
    line1.b = Vec2{10, 0};
    sketch.entities.addLine(line1);
    
    // Second line: diagonal from (0, 5) to (8, 8)
    Line2D line2;
    line2.h.id = sketch.nextEntityId++;
    line2.a = Vec2{0, 5};
    line2.b = Vec2{8, 8};
    auto line2Handle = sketch.entities.addLine(line2);
    
    // Add parallel constraint
    GeometricConstraint parallel;
    parallel.meta.id = sketch.nextConstraintId++;
    parallel.type = GeometricConstraintType::Parallel;
    parallel.refs.push_back({line1.h.id, EntityAnchor::None});
    parallel.refs.push_back({line2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(parallel);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // Check that line2 is now horizontal (parallel to line1)
    const auto& solved1 = result.sketch.entities.line(0);
    const auto& solved2 = result.sketch.entities.line(line2Handle.index);
    
    // Both should be horizontal
    ASSERT(near(solved1.a.y, solved1.b.y));
    ASSERT(near(solved2.a.y, solved2.b.y));
    
    // Or check direction vectors
    double dx1 = solved1.b.x - solved1.a.x;
    double dy1 = solved1.b.y - solved1.a.y;
    double len1 = std::sqrt(dx1*dx1 + dy1*dy1);
    
    double dx2 = solved2.b.x - solved2.a.x;
    double dy2 = solved2.b.y - solved2.a.y;
    double len2 = std::sqrt(dx2*dx2 + dy2*dy2);
    
    // Unit vectors should be equal or opposite
    double ux1 = dx1 / len1, uy1 = dy1 / len1;
    double ux2 = dx2 / len2, uy2 = dy2 / len2;
    
    double dot = ux1 * ux2 + uy1 * uy2;
    ASSERT(near(std::abs(dot), 1.0)); // Parallel: dot = ±1
}

TEST(ParallelConstraint, DiagonalLines) {
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // First line: 45° diagonal
    Line2D line1;
    line1.h.id = sketch.nextEntityId++;
    line1.a = Vec2{0, 0};
    line1.b = Vec2{10, 10};
    sketch.entities.addLine(line1);
    
    // Second line: different angle
    Line2D line2;
    line2.h.id = sketch.nextEntityId++;
    line2.a = Vec2{20, 0};
    line2.b = Vec2{25, 8};
    auto line2Handle = sketch.entities.addLine(line2);
    
    // Add parallel constraint
    GeometricConstraint parallel;
    parallel.meta.id = sketch.nextConstraintId++;
    parallel.type = GeometricConstraintType::Parallel;
    parallel.refs.push_back({line1.h.id, EntityAnchor::None});
    parallel.refs.push_back({line2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(parallel);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solved1 = result.sketch.entities.line(0);
    const auto& solved2 = result.sketch.entities.line(line2Handle.index);
    
    // Check directions are parallel
    double dx1 = solved1.b.x - solved1.a.x;
    double dy1 = solved1.b.y - solved1.a.y;
    double len1 = std::sqrt(dx1*dx1 + dy1*dy1);
    
    double dx2 = solved2.b.x - solved2.a.x;
    double dy2 = solved2.b.y - solved2.a.y;
    double len2 = std::sqrt(dx2*dx2 + dy2*dy2);
    
    double ux1 = dx1 / len1, uy1 = dy1 / len1;
    double ux2 = dx2 / len2, uy2 = dy2 / len2;
    
    double dot = ux1 * ux2 + uy1 * uy2;
    ASSERT(near(std::abs(dot), 1.0, 0.01)); // Parallel
}


// ============================================================================
// PERPENDICULAR CONSTRAINT TESTS
// ============================================================================

TEST(PerpendicularConstraint, TwoLines) {
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // First line: horizontal
    Line2D line1;
    line1.h.id = sketch.nextEntityId++;
    line1.a = Vec2{0, 0};
    line1.b = Vec2{10, 0};
    sketch.entities.addLine(line1);
    
    // Second line: diagonal (not perpendicular yet)
    Line2D line2;
    line2.h.id = sketch.nextEntityId++;
    line2.a = Vec2{0, 0};
    line2.b = Vec2{5, 8};
    auto line2Handle = sketch.entities.addLine(line2);
    
    // Add perpendicular constraint
    GeometricConstraint perp;
    perp.meta.id = sketch.nextConstraintId++;
    perp.type = GeometricConstraintType::Perpendicular;
    perp.refs.push_back({line1.h.id, EntityAnchor::None});
    perp.refs.push_back({line2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(perp);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solved1 = result.sketch.entities.line(0);
    const auto& solved2 = result.sketch.entities.line(line2Handle.index);
    
    // Check that lines are perpendicular (dot product = 0)
    double dx1 = solved1.b.x - solved1.a.x;
    double dy1 = solved1.b.y - solved1.a.y;
    double len1 = std::sqrt(dx1*dx1 + dy1*dy1);
    
    double dx2 = solved2.b.x - solved2.a.x;
    double dy2 = solved2.b.y - solved2.a.y;
    double len2 = std::sqrt(dx2*dx2 + dy2*dy2);
    
    double ux1 = dx1 / len1, uy1 = dy1 / len1;
    double ux2 = dx2 / len2, uy2 = dy2 / len2;
    
    double dot = ux1 * ux2 + uy1 * uy2;
    ASSERT(near(dot, 0.0, 0.01)); // Perpendicular: dot = 0
}

TEST(PerpendicularConstraint, DiagonalLines) {
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // First line: 45° diagonal
    Line2D line1;
    line1.h.id = sketch.nextEntityId++;
    line1.a = Vec2{0, 0};
    line1.b = Vec2{10, 10};
    sketch.entities.addLine(line1);
    
    // Second line: different angle
    Line2D line2;
    line2.h.id = sketch.nextEntityId++;
    line2.a = Vec2{0, 10};
    line2.b = Vec2{5, 8};
    auto line2Handle = sketch.entities.addLine(line2);
    
    // Add perpendicular constraint
    GeometricConstraint perp;
    perp.meta.id = sketch.nextConstraintId++;
    perp.type = GeometricConstraintType::Perpendicular;
    perp.refs.push_back({line1.h.id, EntityAnchor::None});
    perp.refs.push_back({line2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(perp);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    const auto& solved1 = result.sketch.entities.line(0);
    const auto& solved2 = result.sketch.entities.line(line2Handle.index);
    
    // Check perpendicular
    double dx1 = solved1.b.x - solved1.a.x;
    double dy1 = solved1.b.y - solved1.a.y;
    double len1 = std::sqrt(dx1*dx1 + dy1*dy1);
    
    double dx2 = solved2.b.x - solved2.a.x;
    double dy2 = solved2.b.y - solved2.a.y;
    double len2 = std::sqrt(dx2*dx2 + dy2*dy2);
    
    double ux1 = dx1 / len1, uy1 = dy1 / len1;
    double ux2 = dx2 / len2, uy2 = dy2 / len2;
    
    double dot = ux1 * ux2 + uy1 * uy2;
    ASSERT(near(dot, 0.0, 0.01)); // Perpendicular
}


// ============================================================================
// EQUAL LENGTH CONSTRAINT TESTS
// ============================================================================

TEST(EqualLengthConstraint, TwoLines) {
    // Note: This test assumes you add an Equal constraint type
    // or use a geometric constraint for equal length
    // For now, this is a placeholder
    
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // First line: length 10
    Line2D line1;
    line1.h.id = sketch.nextEntityId++;
    line1.a = Vec2{0, 0};
    line1.b = Vec2{10, 0};
    sketch.entities.addLine(line1);
    
    // Second line: length 5 (will be adjusted to 10)
    Line2D line2;
    line2.h.id = sketch.nextEntityId++;
    line2.a = Vec2{0, 5};
    line2.b = Vec2{5, 5};
    auto line2Handle = sketch.entities.addLine(line2);
    
    // TODO: Add equal length constraint when implemented
    // This could be a GeometricConstraintType::Equal or a dimensional constraint
}


// ============================================================================
// COMBINATION TESTS
// ============================================================================

TEST(CombinedConstraints, RectangleWithAllConstraints) {
    // Test rectangle with horizontal, vertical, and coincident constraints
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // Create 4 lines forming approximate rectangle
    Line2D bottom;
    bottom.h.id = sketch.nextEntityId++;
    bottom.a = Vec2{0, 0};
    bottom.b = Vec2{10.1, 0.2};  // Slightly off
    auto hBottom = sketch.entities.addLine(bottom);
    
    Line2D right;
    right.h.id = sketch.nextEntityId++;
    right.a = Vec2{10, 0};
    right.b = Vec2{10.1, 5.1};
    auto hRight = sketch.entities.addLine(right);
    
    Line2D top;
    top.h.id = sketch.nextEntityId++;
    top.a = Vec2{10, 5};
    top.b = Vec2{0.1, 5.1};
    auto hTop = sketch.entities.addLine(top);
    
    Line2D left;
    left.h.id = sketch.nextEntityId++;
    left.a = Vec2{0, 5};
    left.b = Vec2{0.1, 0.1};
    auto hLeft = sketch.entities.addLine(left);
    
    // Add horizontal constraints
    GeometricConstraint bottomHoriz;
    bottomHoriz.meta.id = sketch.nextConstraintId++;
    bottomHoriz.type = GeometricConstraintType::Horizontal;
    bottomHoriz.refs.push_back({bottom.h.id, EntityAnchor::None});
    sketch.constraints.push_back(bottomHoriz);
    
    GeometricConstraint topHoriz;
    topHoriz.meta.id = sketch.nextConstraintId++;
    topHoriz.type = GeometricConstraintType::Horizontal;
    topHoriz.refs.push_back({top.h.id, EntityAnchor::None});
    sketch.constraints.push_back(topHoriz);
    
    // Add vertical constraints
    GeometricConstraint rightVert;
    rightVert.meta.id = sketch.nextConstraintId++;
    rightVert.type = GeometricConstraintType::Vertical;
    rightVert.refs.push_back({right.h.id, EntityAnchor::None});
    sketch.constraints.push_back(rightVert);
    
    GeometricConstraint leftVert;
    leftVert.meta.id = sketch.nextConstraintId++;
    leftVert.type = GeometricConstraintType::Vertical;
    leftVert.refs.push_back({left.h.id, EntityAnchor::None});
    sketch.constraints.push_back(leftVert);
    
    // Add coincident at corners
    GeometricConstraint corner1;
    corner1.meta.id = sketch.nextConstraintId++;
    corner1.type = GeometricConstraintType::Coincident;
    corner1.refs.push_back({bottom.h.id, EntityAnchor::LineEnd});
    corner1.refs.push_back({right.h.id, EntityAnchor::LineStart});
    sketch.constraints.push_back(corner1);
    
    GeometricConstraint corner2;
    corner2.meta.id = sketch.nextConstraintId++;
    corner2.type = GeometricConstraintType::Coincident;
    corner2.refs.push_back({right.h.id, EntityAnchor::LineEnd});
    corner2.refs.push_back({top.h.id, EntityAnchor::LineStart});
    sketch.constraints.push_back(corner2);
    
    GeometricConstraint corner3;
    corner3.meta.id = sketch.nextConstraintId++;
    corner3.type = GeometricConstraintType::Coincident;
    corner3.refs.push_back({top.h.id, EntityAnchor::LineEnd});
    corner3.refs.push_back({left.h.id, EntityAnchor::LineStart});
    sketch.constraints.push_back(corner3);
    
    GeometricConstraint corner4;
    corner4.meta.id = sketch.nextConstraintId++;
    corner4.type = GeometricConstraintType::Coincident;
    corner4.refs.push_back({left.h.id, EntityAnchor::LineEnd});
    corner4.refs.push_back({bottom.h.id, EntityAnchor::LineStart});
    sketch.constraints.push_back(corner4);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // Verify rectangle
    const auto& sBottom = result.sketch.entities.line(hBottom.index);
    const auto& sRight = result.sketch.entities.line(hRight.index);
    const auto& sTop = result.sketch.entities.line(hTop.index);
    const auto& sLeft = result.sketch.entities.line(hLeft.index);
    
    // Check horizontal/vertical
    ASSERT(near(sBottom.a.y, sBottom.b.y)); // horizontal
    ASSERT(near(sTop.a.y, sTop.b.y));       // horizontal
    ASSERT(near(sRight.a.x, sRight.b.x));   // vertical
    ASSERT(near(sLeft.a.x, sLeft.b.x));     // vertical
    
    // Check corners
    ASSERT(near(sBottom.b, sRight.a));
    ASSERT(near(sRight.b, sTop.a));
    ASSERT(near(sTop.b, sLeft.a));
    ASSERT(near(sLeft.b, sBottom.a));
}

TEST(CombinedConstraints, ParallelAndPerpendicular) {
    // Create two pairs of parallel lines that are perpendicular to each other
    Sketch sketch;
    sketch.nextEntityId = 1;
    sketch.nextConstraintId = 1;
    
    // First pair (horizontal)
    Line2D h1;
    h1.h.id = sketch.nextEntityId++;
    h1.a = Vec2{0, 0};
    h1.b = Vec2{10, 0};
    sketch.entities.addLine(h1);
    
    Line2D h2;
    h2.h.id = sketch.nextEntityId++;
    h2.a = Vec2{0, 5};
    h2.b = Vec2{8, 6};  // Not parallel yet
    sketch.entities.addLine(h2);
    
    // Second pair (will be vertical)
    Line2D v1;
    v1.h.id = sketch.nextEntityId++;
    v1.a = Vec2{0, 0};
    v1.b = Vec2{1, 10};  // Not vertical yet
    sketch.entities.addLine(v1);
    
    Line2D v2;
    v2.h.id = sketch.nextEntityId++;
    v2.a = Vec2{5, 0};
    v2.b = Vec2{6, 8};  // Not vertical yet
    sketch.entities.addLine(v2);
    
    // Make h1 and h2 parallel
    GeometricConstraint par1;
    par1.meta.id = sketch.nextConstraintId++;
    par1.type = GeometricConstraintType::Parallel;
    par1.refs.push_back({h1.h.id, EntityAnchor::None});
    par1.refs.push_back({h2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(par1);
    
    // Make v1 and v2 parallel
    GeometricConstraint par2;
    par2.meta.id = sketch.nextConstraintId++;
    par2.type = GeometricConstraintType::Parallel;
    par2.refs.push_back({v1.h.id, EntityAnchor::None});
    par2.refs.push_back({v2.h.id, EntityAnchor::None});
    sketch.constraints.push_back(par2);
    
    // Make h1 perpendicular to v1
    GeometricConstraint perp;
    perp.meta.id = sketch.nextConstraintId++;
    perp.type = GeometricConstraintType::Perpendicular;
    perp.refs.push_back({h1.h.id, EntityAnchor::None});
    perp.refs.push_back({v1.h.id, EntityAnchor::None});
    sketch.constraints.push_back(perp);
    
    // Solve
    adapters::solver::BasicConstraintSolver solver;
    auto result = solver.solve(sketch);
    
    ASSERT(result.report.converged);
    
    // All constraints should be satisfied
    // h1 and h2 parallel, v1 and v2 parallel, and the two pairs perpendicular
}

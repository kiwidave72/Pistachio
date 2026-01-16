#include "tests/mini_test.h"

#include "domain/SketchModel.h"
#include "domain/SketchConstraints.h"
#include "adapters/solvers/BasicConstraintSolver.h"

using namespace domain::sketch;

static Sketch MakeEmptySketch() {
    Sketch s;
    s.id = 1;
    s.name = "TestSketch";
    s.visible = true;
    s.nextEntityId = 1;
    s.nextConstraintId = 1;
    return s;
}

static EntityRef Ref(EntityId id, EntityAnchor a = EntityAnchor::None) {
    return EntityRef{ id, a };
}

TEST_CASE(Horizontal_Legacy_LineRef)
{
    auto s = MakeEmptySketch();

    Line2D ln;
    ln.h.id = s.nextEntityId++;
    ln.a = Vec2{ 0.0, 0.0 };
    ln.b = Vec2{ 10.0, 5.0 };
    s.entities.addLine(ln);

    GeometricConstraint gc;
    gc.meta.id = s.nextConstraintId++;
    gc.meta.name = "Horizontal";
    gc.type = GeometricConstraintType::Horizontal;
    gc.refs = { Ref(ln.h.id, EntityAnchor::None) }; // legacy format
    s.constraints.emplace_back(gc);

    adapters::solver::BasicConstraintSolver solver;
    auto out = solver.solve(s);

    REQUIRE(out.report.converged);

    auto h = out.sketch.entities.getHandle(ln.h.id);
    REQUIRE(h.kind == EntityKind::Line);
    const auto& l2 = out.sketch.entities.line(h.index);
    REQUIRE_CLOSE(l2.b.y, l2.a.y, 1e-9);
}

TEST_CASE(Vertical_Anchored_LineStart_LineEnd)
{
    auto s = MakeEmptySketch();

    Line2D ln;
    ln.h.id = s.nextEntityId++;
    ln.a = Vec2{ 2.0, 0.0 };
    ln.b = Vec2{ 7.0, 5.0 };
    s.entities.addLine(ln);

    GeometricConstraint gc;
    gc.meta.id = s.nextConstraintId++;
    gc.meta.name = "Vertical";
    gc.type = GeometricConstraintType::Vertical;
    gc.refs = { Ref(ln.h.id, EntityAnchor::LineStart), Ref(ln.h.id, EntityAnchor::LineEnd) };
    s.constraints.emplace_back(gc);

    adapters::solver::BasicConstraintSolver solver;
    auto out = solver.solve(s);

    REQUIRE(out.report.converged);

    auto h = out.sketch.entities.getHandle(ln.h.id);
    const auto& l2 = out.sketch.entities.line(h.index);
    REQUIRE_CLOSE(l2.b.x, l2.a.x, 1e-9);
}

TEST_CASE(Coincident_Point_Point)
{
    auto s = MakeEmptySketch();

    Point2D p1;
    p1.h.id = s.nextEntityId++;
    p1.p = Vec2{ 1.0, 2.0 };

    Point2D p2;
    p2.h.id = s.nextEntityId++;
    p2.p = Vec2{ 5.0, 6.0 };

    s.entities.addPoint(p1);
    s.entities.addPoint(p2);

    GeometricConstraint gc;
    gc.meta.id = s.nextConstraintId++;
    gc.meta.name = "Coincident";
    gc.type = GeometricConstraintType::Coincident;
    gc.refs = { Ref(p1.h.id), Ref(p2.h.id) };
    s.constraints.emplace_back(gc);

    adapters::solver::BasicConstraintSolver solver;
    auto out = solver.solve(s);
    REQUIRE(out.report.converged);

    auto h1 = out.sketch.entities.getHandle(p1.h.id);
    auto h2 = out.sketch.entities.getHandle(p2.h.id);

    const auto& rp1 = out.sketch.entities.point(h1.index);
    const auto& rp2 = out.sketch.entities.point(h2.index);

    REQUIRE_CLOSE(rp2.p.x, rp1.p.x, 1e-9);
    REQUIRE_CLOSE(rp2.p.y, rp1.p.y, 1e-9);
}

TEST_CASE(Tangent_Line_Circle)
{
    auto s = MakeEmptySketch();

    Line2D ln;
    ln.h.id = s.nextEntityId++;
    ln.a = Vec2{ -10.0, 0.0 };
    ln.b = Vec2{  10.0, 0.0 };
    s.entities.addLine(ln);

    Circle2D c;
    c.h.id = s.nextEntityId++;
    c.center = Vec2{ 0.0, 5.0 };
    c.radius = 3.0;
    s.entities.addCircle(c);

    GeometricConstraint gc;
    gc.meta.id = s.nextConstraintId++;
    gc.meta.name = "Tangent";
    gc.type = GeometricConstraintType::Tangent;
    gc.refs = { Ref(ln.h.id), Ref(c.h.id) };
    s.constraints.emplace_back(gc);

    adapters::solver::BasicConstraintSolver solver;
    auto out = solver.solve(s);
    REQUIRE(out.report.converged);

    auto hc = out.sketch.entities.getHandle(c.h.id);
    const auto& rc = out.sketch.entities.circle(hc.index);

    // Expected: center moves from y=5 to y=3 (tangent above the line)
    REQUIRE_CLOSE(rc.center.x, 0.0, 1e-6);
    REQUIRE_CLOSE(rc.center.y, 3.0, 1e-6);
}

TEST_CASE(Tangent_Circle_Circle)
{
    auto s = MakeEmptySketch();

    Circle2D c1;
    c1.h.id = s.nextEntityId++;
    c1.center = Vec2{ 0.0, 0.0 };
    c1.radius = 2.0;
    s.entities.addCircle(c1);

    Circle2D c2;
    c2.h.id = s.nextEntityId++;
    c2.center = Vec2{ 10.0, 0.0 };
    c2.radius = 1.0;
    s.entities.addCircle(c2);

    GeometricConstraint gc;
    gc.meta.id = s.nextConstraintId++;
    gc.meta.name = "Tangent";
    gc.type = GeometricConstraintType::Tangent;
    gc.refs = { Ref(c1.h.id), Ref(c2.h.id) };
    s.constraints.emplace_back(gc);

    adapters::solver::BasicConstraintSolver solver;
    auto out = solver.solve(s);
    REQUIRE(out.report.converged);

    auto h2 = out.sketch.entities.getHandle(c2.h.id);
    const auto& rc2 = out.sketch.entities.circle(h2.index);

    // External tangency: distance == r1+r2 => 3.0, so x should move from 10 to 3
    REQUIRE_CLOSE(rc2.center.x, 3.0, 1e-6);
    REQUIRE_CLOSE(rc2.center.y, 0.0, 1e-6);
}

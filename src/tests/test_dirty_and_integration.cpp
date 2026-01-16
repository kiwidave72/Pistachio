#include "tests/mini_test.h"

#include "core/SketchDirtyState.h"
#include "domain/SketchModel.h"
#include "domain/SketchConstraints.h"
#include "adapters/solvers/BasicConstraintSolver.h"

using namespace domain::sketch;

static Sketch MakeSketch_LineWithHorizontalConstraint() {
    Sketch s;
    s.id = 1;
    s.name = "DirtySketch";

    Line2D ln;
    ln.h.id = s.nextEntityId++;
    ln.a = Vec2{ 0.0, 0.0 };
    ln.b = Vec2{ 10.0, 5.0 };
    s.entities.addLine(ln);

    GeometricConstraint gc;
    gc.meta.id = s.nextConstraintId++;
    gc.meta.name = "Horizontal";
    gc.type = GeometricConstraintType::Horizontal;
    gc.refs = { EntityRef{ ln.h.id, EntityAnchor::None } };
    s.constraints.emplace_back(gc);

    return s;
}

TEST_CASE(DirtyState_MarkDirty_sets_flag_and_increments_serial)
{
    core::SketchDirtyState d;
    REQUIRE(!d.needsSolve);
    REQUIRE(d.changeSerial == 0);

    d.MarkDirty();
    REQUIRE(d.needsSolve);
    REQUIRE(d.changeSerial == 1);

    d.MarkDirty();
    REQUIRE(d.needsSolve);
    REQUIRE(d.changeSerial == 2);

    d.Clear();
    REQUIRE(!d.needsSolve);
    REQUIRE(d.changeSerial == 2);
}

TEST_CASE(Integration_SolveOnlyWhenDirty)
{
    core::SketchDirtyState d;
    auto s = MakeSketch_LineWithHorizontalConstraint();

    // Not dirty: nothing happens (we simulate a controller that only runs solver when dirty)
    adapters::solver::BasicConstraintSolver solver;

    auto controllerTick = [&](Sketch& sk) {
        if (!d.needsSolve) return;
        auto out = solver.solve(sk);
        REQUIRE(out.report.converged);
        sk = out.sketch; // emulate UI/app applying the resolved sketch
        d.Clear();
    };

    controllerTick(s);

    // Now mark dirty and expect solve to happen
    d.MarkDirty();
    controllerTick(s);

    auto h = s.entities.getHandle(1);
    const auto& l2 = s.entities.line(h.index);
    REQUIRE_CLOSE(l2.a.y, l2.b.y, 1e-9);
    REQUIRE(!d.needsSolve);
}

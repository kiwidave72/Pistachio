#include "core/rendering/SketchRenderBuilder.h"

namespace
{
    static inline glm::vec3 ToWorld(float x, float y, float z)
    {
        return { x, y, z };
    }

    static inline core::rendering::RenderId ToRenderId(domain::sketch::EntityId id)
    {
        return static_cast<core::rendering::RenderId>(id);
    }
}

namespace core::rendering
{
    RenderScene BuildRenderSceneFromSketch(const domain::sketch::Sketch& sketch, const SketchRenderOptions& opt)
    {
        RenderScene scene;
        scene.Clear();

        // Grid on XY plane
        scene.grid.origin = { 0,0,opt.z };
        scene.grid.uAxis = { 1,0,0 };
        scene.grid.vAxis = { 0,1,0 };
        scene.grid.spacing = 10.0f;
        scene.grid.lineCount = 40;
        scene.showGrid = true;

        const auto& store = sketch.entities;

        // --- Points ---
        for (auto const& p : store.points())
        {
            if (!p.h.visible) continue;

            Point3D pt;
            pt.id = ToRenderId(p.h.id);
            pt.p = ToWorld((float)p.p.x, (float)p.p.y, opt.z);
            pt.size = opt.pointSize;
            pt.color = opt.pointColor;
            pt.selectable = p.h.selectable;
            scene.points.push_back(pt);
        }

        // --- Lines ---
        for (auto const& l : store.lines())
        {
            if (!l.h.visible) continue;

            Line3D ln;
            ln.id = ToRenderId(l.h.id);
            ln.a = ToWorld((float)l.a.x, (float)l.a.y, opt.z);
            ln.b = ToWorld((float)l.b.x, (float)l.b.y, opt.z);
            ln.thickness = opt.lineThickness;
            ln.color = l.h.construction ? opt.constructionColor : opt.entityColor;
            ln.selectable = l.h.selectable;
            scene.lines.push_back(ln);
        }

        // --- Circles (TRUE) ---
        for (auto const& c : store.circles())
        {
            if (!c.h.visible) continue;

            Circle3D cc;
            cc.id = ToRenderId(c.h.id);
            cc.center = ToWorld((float)c.center.x, (float)c.center.y, opt.z);
            cc.normal = { 0,0,1 };
            cc.radius = (float)c.radius;
            cc.thickness = opt.lineThickness;
            cc.construction = c.h.construction;
            cc.color = c.h.construction ? opt.constructionColor : opt.entityColor;
            cc.selectable = c.h.selectable;

            scene.circles.push_back(cc);
        }

        // --- Arcs (TRUE) ---
        for (auto const& a : store.arcs())
        {
            if (!a.h.visible) continue;

            Arc3D ar;
            ar.id = ToRenderId(a.h.id);
            ar.center = ToWorld((float)a.center.x, (float)a.center.y, opt.z);
            ar.normal = { 0,0,1 };
            ar.radius = (float)a.radius;
            ar.start = ToWorld((float)a.start.x, (float)a.start.y, opt.z);
            ar.end = ToWorld((float)a.end.x, (float)a.end.y, opt.z);
            ar.ccw = a.ccw;
            ar.thickness = opt.lineThickness;
            ar.construction = a.h.construction;
            ar.color = a.h.construction ? opt.constructionColor : opt.entityColor;
            ar.selectable = a.h.selectable;

            scene.arcs.push_back(ar);
        }

        // --- Ellipses (TRUE) ---
        for (auto const& e : store.ellipses())
        {
            if (!e.h.visible) continue;

            Ellipse3D el;
            el.id = ToRenderId(e.h.id);
            el.center = ToWorld((float)e.center.x, (float)e.center.y, opt.z);
            el.normal = { 0,0,1 };
            el.rx = (float)e.rx;
            el.ry = (float)e.ry;
            el.rotationRad = (float)e.rotation; // assumes your entity stores radians
            el.thickness = opt.lineThickness;
            el.construction = e.h.construction;
            el.color = e.h.construction ? opt.constructionColor : opt.entityColor;
            el.selectable = e.h.selectable;

            scene.ellipses.push_back(el);
        }

        // --- Curves (TEMP: polyline from control points) ---
        for (auto const& cv : store.curves())
        {
            if (!cv.h.visible) continue;
            if (cv.controlPoints.size() < 2) continue;

            Polyline3D pl;
            pl.id = ToRenderId(cv.h.id);
            pl.color = cv.h.construction ? opt.constructionColor : opt.entityColor;
            pl.thickness = opt.lineThickness;

            pl.points.reserve(cv.controlPoints.size() + (cv.closed ? 1 : 0));
            for (auto const& p : cv.controlPoints)
                pl.points.push_back(ToWorld((float)p.x, (float)p.y, opt.z));

            if (cv.closed)
                pl.points.push_back(pl.points.front());

            scene.polylines.push_back(std::move(pl));
        }

        return scene;
    }
}

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "SketchIds.h"
#include "SketchMath.h"

namespace domain::sketch {

    // Runtime-optimized entity storage:
    //  - Each entity type has its own contiguous vector.
    //  - EntityId -> (kind,index) map for O(1) access.
    // This is faster for editing + constraint solving than storing heterogeneous entities in a single variant list.

    struct EntityHeader {
        EntityId id{};
        std::string name;
        bool construction{ false };
        bool visible{ true };
        bool selectable{ true };
    };

    struct Point2D {
        EntityHeader h;
        Vec2 p;
    };

    struct Line2D {
        EntityHeader h;
        Vec2 a; // start
        Vec2 b; // end
    };

    struct Circle2D {
        EntityHeader h;
        Vec2 center;
        double radius{ 1.0 };
    };

    struct Arc2D {
        EntityHeader h;
        Vec2 center;
        double radius{ 1.0 };
        Vec2 start;
        Vec2 end;
        bool ccw{ true };
    };

    struct Ellipse2D {
        EntityHeader h;
        Vec2 center;
        double rx{ 2.0 };
        double ry{ 1.0 };
        double rotation{ 0.0 }; // radians
    };

    struct Curve2D {
        EntityHeader h;
        std::vector<Vec2> controlPoints;
        bool closed{ false };
    };

    struct EntityHandle {
        EntityKind kind{ EntityKind::Point };
        std::uint32_t index{ 0 };
    };

    class EntityStore {
    public:
        // Insert helpers. Returns handle for fast access.
        EntityHandle addPoint(Point2D p);
        EntityHandle addLine(Line2D l);
        EntityHandle addCircle(Circle2D c);
        EntityHandle addArc(Arc2D a);
        EntityHandle addEllipse(Ellipse2D e);
        EntityHandle addCurve(Curve2D c);

        bool contains(EntityId id) const;
        EntityHandle getHandle(EntityId id) const; // throws std::out_of_range if missing

        // Typed access
        Point2D& point(std::uint32_t idx) { return m_points.at(idx); }
        const Point2D& point(std::uint32_t idx) const { return m_points.at(idx); }

        Line2D& line(std::uint32_t idx) { return m_lines.at(idx); }
        const Line2D& line(std::uint32_t idx) const { return m_lines.at(idx); }

        Circle2D& circle(std::uint32_t idx) { return m_circles.at(idx); }
        const Circle2D& circle(std::uint32_t idx) const { return m_circles.at(idx); }

        Arc2D& arc(std::uint32_t idx) { return m_arcs.at(idx); }
        const Arc2D& arc(std::uint32_t idx) const { return m_arcs.at(idx); }

        Ellipse2D& ellipse(std::uint32_t idx) { return m_ellipses.at(idx); }
        const Ellipse2D& ellipse(std::uint32_t idx) const { return m_ellipses.at(idx); }

        Curve2D& curve(std::uint32_t idx) { return m_curves.at(idx); }
        const Curve2D& curve(std::uint32_t idx) const { return m_curves.at(idx); }

        // Bulk read access (useful for rendering)
        const std::vector<Point2D>& points() const { return m_points; }
        const std::vector<Line2D>& lines() const { return m_lines; }
        const std::vector<Circle2D>& circles() const { return m_circles; }
        const std::vector<Arc2D>& arcs() const { return m_arcs; }
        const std::vector<Ellipse2D>& ellipses() const { return m_ellipses; }
        const std::vector<Curve2D>& curves() const { return m_curves; }

        void clear();

    private:
        std::vector<Point2D> m_points;
        std::vector<Line2D> m_lines;
        std::vector<Circle2D> m_circles;
        std::vector<Arc2D> m_arcs;
        std::vector<Ellipse2D> m_ellipses;
        std::vector<Curve2D> m_curves;

        std::unordered_map<EntityId, EntityHandle> m_idToHandle;
    };

} // namespace domain::sketch

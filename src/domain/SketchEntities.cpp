#include "SketchEntities.h"

#include <stdexcept>

namespace domain::sketch {

    static void ensureUnique(const std::unordered_map<EntityId, EntityHandle>& map, EntityId id) {
        if (map.find(id) != map.end()) {
            throw std::runtime_error("Duplicate EntityId encountered while building EntityStore");
        }
    }

    EntityHandle EntityStore::addPoint(Point2D p) {
        ensureUnique(m_idToHandle, p.h.id);
        EntityHandle h{ EntityKind::Point, static_cast<std::uint32_t>(m_points.size()) };
        m_points.emplace_back(std::move(p));
        m_idToHandle.emplace(m_points.back().h.id, h);
        return h;
    }

    EntityHandle EntityStore::addLine(Line2D l) {
        ensureUnique(m_idToHandle, l.h.id);
        EntityHandle h{ EntityKind::Line, static_cast<std::uint32_t>(m_lines.size()) };
        m_lines.emplace_back(std::move(l));
        m_idToHandle.emplace(m_lines.back().h.id, h);
        return h;
    }

    EntityHandle EntityStore::addCircle(Circle2D c) {
        ensureUnique(m_idToHandle, c.h.id);
        EntityHandle h{ EntityKind::Circle, static_cast<std::uint32_t>(m_circles.size()) };
        m_circles.emplace_back(std::move(c));
        m_idToHandle.emplace(m_circles.back().h.id, h);
        return h;
    }

    EntityHandle EntityStore::addArc(Arc2D a) {
        ensureUnique(m_idToHandle, a.h.id);
        EntityHandle h{ EntityKind::Arc, static_cast<std::uint32_t>(m_arcs.size()) };
        m_arcs.emplace_back(std::move(a));
        m_idToHandle.emplace(m_arcs.back().h.id, h);
        return h;
    }

    EntityHandle EntityStore::addEllipse(Ellipse2D e) {
        ensureUnique(m_idToHandle, e.h.id);
        EntityHandle h{ EntityKind::Ellipse, static_cast<std::uint32_t>(m_ellipses.size()) };
        m_ellipses.emplace_back(std::move(e));
        m_idToHandle.emplace(m_ellipses.back().h.id, h);
        return h;
    }

    EntityHandle EntityStore::addCurve(Curve2D c) {
        ensureUnique(m_idToHandle, c.h.id);
        EntityHandle h{ EntityKind::Curve, static_cast<std::uint32_t>(m_curves.size()) };
        m_curves.emplace_back(std::move(c));
        m_idToHandle.emplace(m_curves.back().h.id, h);
        return h;
    }

    bool EntityStore::contains(EntityId id) const {
        return m_idToHandle.find(id) != m_idToHandle.end();
    }

    EntityHandle EntityStore::getHandle(EntityId id) const {
        auto it = m_idToHandle.find(id);
        if (it == m_idToHandle.end()) {
            throw std::out_of_range("EntityId not found");
        }
        return it->second;
    }

    void EntityStore::clear() {
        m_points.clear();
        m_lines.clear();
        m_circles.clear();
        m_arcs.clear();
        m_ellipses.clear();
        m_curves.clear();
        m_idToHandle.clear();
    }

} // namespace domain::sketch

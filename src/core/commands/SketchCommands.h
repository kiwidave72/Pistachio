#pragma once

#include "core/commands/ICommand.h"
#include "domain/SketchModel.h"
#include "domain/SketchConstraints.h"

#include <optional>
#include <variant>
#include <vector>
#include <algorithm>

namespace core::commands {

    // Adds a Line2D into a sketch (single undo step).
class AddLine2DCommand final : public ICommand {
public:
    AddLine2DCommand(domain::sketch::Sketch& sketch, domain::sketch::Vec2 a, domain::sketch::Vec2 b)
        : m_sketch(sketch), m_a(a), m_b(b) {}

    const char* Name() const override { return "Add Line"; }

    void Do() override
    {
        if (m_id == 0)
            m_id = m_sketch.nextEntityId++;

        domain::sketch::Line2D l;
        l.h.id = m_id;
        l.a = m_a;
        l.b = m_b;

        m_sketch.entities.addLine(std::move(l));
    }

    void Undo() override
    {
        m_sketch.entities.remove(m_id);
    }

private:
    domain::sketch::Sketch& m_sketch;
    domain::sketch::Vec2 m_a{};
    domain::sketch::Vec2 m_b{};
    domain::sketch::EntityId m_id = 0;
};


    // Adds a Circle2D into a sketch (single undo step).
    class AddCircle2DCommand final : public ICommand {
    public:
        AddCircle2DCommand(domain::sketch::Sketch& sketch, domain::sketch::Vec2 center, double radius)
            : m_sketch(sketch), m_center(center), m_radius(radius) {}

        const char* Name() const override { return "Add Circle"; }

        void Do() override
        {
            if (m_id == 0)
                m_id = m_sketch.nextEntityId++;

            domain::sketch::Circle2D c;
            c.h.id = m_id;
            c.center = m_center;
            c.radius = m_radius;

            m_sketch.entities.addCircle(std::move(c));
        }

        void Undo() override
        {
            m_sketch.entities.remove(m_id);
        }

    private:
        domain::sketch::Sketch& m_sketch;
        domain::sketch::Vec2 m_center{};
        double m_radius = 1.0;
        domain::sketch::EntityId m_id = 0;
    };



    // Adds a geometric constraint (stored in Sketch::constraints as a variant)
class AddGeometricConstraintCommand final : public ICommand
{
public:
    AddGeometricConstraintCommand(domain::sketch::Sketch& sketch, domain::sketch::GeometricConstraint c)
        : m_sketch(sketch), m_constraint(std::move(c)) {}

    const char* Name() const override { return "Add Constraint"; }

    void Do() override
    {
        if (m_constraint.meta.id == 0)
            m_constraint.meta.id = m_sketch.nextConstraintId++;

        m_id = m_constraint.meta.id;
        m_sketch.constraints.emplace_back(m_constraint);
    }

    void Undo() override
    {
        auto& v = m_sketch.constraints;
        for (auto it = v.begin(); it != v.end(); ++it)
        {
            domain::sketch::ConstraintId cid = std::visit([](auto& c) { return c.meta.id; }, *it);
            if (cid == m_id)
            {
                v.erase(it);
                break;
            }
        }
    }

private:
    domain::sketch::Sketch& m_sketch;
    domain::sketch::GeometricConstraint m_constraint{};
    domain::sketch::ConstraintId m_id{ 0 };
};


    // Deletes a single entity (and any constraints that reference it) from a sketch.
    // Supports undo/redo by storing a snapshot of the entity and removed constraints.
    class DeleteEntityCommand final : public ICommand {
    public:
        DeleteEntityCommand(domain::sketch::Sketch& sketch, domain::sketch::EntityId id)
            : m_sketch(sketch), m_id(id) {}

        const char* Name() const override { return "Delete Entity"; }

        void Do() override
        {
            if (m_id == 0) return;

            // Capture snapshot on first execution only.
            if (!m_hasSnapshot)
            {
                CaptureEntitySnapshot();
                CaptureAndRemoveReferencingConstraints();
                m_hasSnapshot = true;
            }
            else
            {
                // Redo: remove again (constraints too)
                CaptureAndRemoveReferencingConstraints();
            }

            // Remove entity (if it already vanished, that's ok)
            m_sketch.entities.remove(m_id);
        }

        void Undo() override
        {
            if (!m_hasSnapshot) return;

            RestoreEntitySnapshot();
            RestoreConstraints();
        }

    private:
        using EntitySnapshot = std::variant<
            domain::sketch::Point2D,
            domain::sketch::Line2D,
            domain::sketch::Circle2D,
            domain::sketch::Arc2D,
            domain::sketch::Ellipse2D,
            domain::sketch::Curve2D
        >;

        void CaptureEntitySnapshot()
        {
            if (!m_sketch.entities.contains(m_id))
                return;

            auto h = m_sketch.entities.getHandle(m_id);
            switch (h.kind)
            {
            case domain::sketch::EntityKind::Point:   m_entity = m_sketch.entities.point(h.index); break;
            case domain::sketch::EntityKind::Line:    m_entity = m_sketch.entities.line(h.index); break;
            case domain::sketch::EntityKind::Circle:  m_entity = m_sketch.entities.circle(h.index); break;
            case domain::sketch::EntityKind::Arc:     m_entity = m_sketch.entities.arc(h.index); break;
            case domain::sketch::EntityKind::Ellipse: m_entity = m_sketch.entities.ellipse(h.index); break;
            case domain::sketch::EntityKind::Curve:   m_entity = m_sketch.entities.curve(h.index); break;
            default: break;
            }
        }

        static bool ConstraintReferences(const domain::sketch::Constraint& c, domain::sketch::EntityId id)
        {
            return std::visit([&](auto&& cc) {
                for (const auto& r : cc.refs)
                    if (r.id == id) return true;
                return false;
            }, c);
        }

        void CaptureAndRemoveReferencingConstraints()
        {
            m_removedConstraints.clear();

            // Walk backwards so indices remain valid.
            for (size_t i = m_sketch.constraints.size(); i-- > 0;)
            {
                if (ConstraintReferences(m_sketch.constraints[i], m_id))
                {
                    m_removedConstraints.emplace_back(i, m_sketch.constraints[i]);
                    m_sketch.constraints.erase(m_sketch.constraints.begin() + (std::ptrdiff_t)i);
                }
            }
        }

        void RestoreEntitySnapshot()
        {
            if (!m_entity.has_value())
                return;

            std::visit([&](auto&& e) {
                using T = std::decay_t<decltype(e)>;
                if constexpr (std::is_same_v<T, domain::sketch::Point2D>)   m_sketch.entities.addPoint(e);
                else if constexpr (std::is_same_v<T, domain::sketch::Line2D>)    m_sketch.entities.addLine(e);
                else if constexpr (std::is_same_v<T, domain::sketch::Circle2D>)  m_sketch.entities.addCircle(e);
                else if constexpr (std::is_same_v<T, domain::sketch::Arc2D>)     m_sketch.entities.addArc(e);
                else if constexpr (std::is_same_v<T, domain::sketch::Ellipse2D>) m_sketch.entities.addEllipse(e);
                else if constexpr (std::is_same_v<T, domain::sketch::Curve2D>)   m_sketch.entities.addCurve(e);
            }, *m_entity);

            // Ensure next id won't collide in future creations.
            if (m_sketch.nextEntityId <= m_id)
                m_sketch.nextEntityId = m_id + 1;
        }

        void RestoreConstraints()
        {
            if (m_removedConstraints.empty())
                return;

            // Insert in ascending index order.
            std::sort(m_removedConstraints.begin(), m_removedConstraints.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            for (const auto& [idx, c] : m_removedConstraints)
            {
                const size_t ins = (std::min)(idx, m_sketch.constraints.size());
                m_sketch.constraints.insert(m_sketch.constraints.begin() + (std::ptrdiff_t)ins, c);

                // Ensure nextConstraintId won't collide.
                std::visit([&](auto&& cc) {
                    if (m_sketch.nextConstraintId <= cc.meta.id)
                        m_sketch.nextConstraintId = cc.meta.id + 1;
                }, c);
            }
        }

    private:
        domain::sketch::Sketch& m_sketch;
        domain::sketch::EntityId m_id{ 0 };

        bool m_hasSnapshot{ false };
        std::optional<EntitySnapshot> m_entity;

        // (originalIndex, constraint)
        std::vector<std::pair<size_t, domain::sketch::Constraint>> m_removedConstraints;
    };


} // namespace core::commands

#pragma once

#include "core/commands/ICommand.h"
#include "domain/SketchModel.h"
#include "domain/SketchConstraints.h"

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


} // namespace core::commands

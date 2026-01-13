#pragma once

#include "core/commands/ICommand.h"
#include "domain/SketchModel.h"

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
            l.h.name = "Line";
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
            c.h.name = "Circle";
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
        double m_radius{ 1.0 };
        domain::sketch::EntityId m_id = 0;
    };

} // namespace core::commands

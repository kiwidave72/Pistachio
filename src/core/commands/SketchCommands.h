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

} // namespace core::commands

#pragma once

#include <memory>
#include <vector>

#include "ICommand.h"

namespace core::commands {

    class CommandHistory {
    public:
        void Execute(std::unique_ptr<ICommand> cmd)
        {
            if (!cmd) return;

            // If a new command is executed after undo, drop the redo branch.
            if (m_index < m_commands.size())
                m_commands.erase(m_commands.begin() + static_cast<std::ptrdiff_t>(m_index), m_commands.end());

            cmd->Do();
            m_commands.push_back(std::move(cmd));
            m_index = m_commands.size();
        }

        bool CanUndo() const { return m_index > 0; }
        bool CanRedo() const { return m_index < m_commands.size(); }

        void Undo()
        {
            if (!CanUndo()) return;
            --m_index;
            m_commands[m_index]->Undo();
        }

        void Redo()
        {
            if (!CanRedo()) return;
            m_commands[m_index]->Do();
            ++m_index;
        }

        void Clear()
        {
            m_commands.clear();
            m_index = 0;
        }

    private:
        std::vector<std::unique_ptr<ICommand>> m_commands;
        size_t m_index = 0;
    };

} // namespace core::commands

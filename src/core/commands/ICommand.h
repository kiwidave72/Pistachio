#pragma once

namespace core::commands {

    struct ICommand {
        virtual ~ICommand() = default;
        virtual void Do() = 0;
        virtual void Undo() = 0;
        virtual const char* Name() const { return "Command"; }
    };

} // namespace core::commands

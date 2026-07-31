#pragma once

// -----------------------------------------------------------------------
// IApplication.h
//
// Narrow interface that plugins depend on instead of the full concrete
// Application class. The only thing a plugin should need from "the app"
// is access to the service registry — everything else (config, task
// runner, event bus, future services) is resolved through it.
//
// Application implements this. UiHostServices::application points at the
// same Application instance, typed through this interface.
// -----------------------------------------------------------------------

namespace core {

    class ServiceRegistry;

    class IApplication
    {
    public:
        virtual ~IApplication() = default;

        virtual ServiceRegistry& services() = 0;
    };

} // namespace core
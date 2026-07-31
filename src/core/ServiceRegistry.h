#pragma once

// -----------------------------------------------------------------------
// ServiceRegistry.h
//
// Owned by Application. Every plugin (service or UI) resolves shared
// systems (config, task runner, event bus, and future services) through
// this, rather than Application/UiHostServices growing a new field per
// system.
//
// - Single-winner registration for now: no priority/override support yet.
//   The API is shaped so resolveAll<T>() can be added later without
//   breaking existing callers.
// - Late-bound resolution is the intended usage pattern: callers should
//   hold the registry (or IApplication) and call resolve<T>() at point of
//   use, every time, rather than caching the resolved pointer long-term.
//   This is what keeps hot-reload safe.
// -----------------------------------------------------------------------

#include <unordered_map>
#include <typeindex>

namespace core {

    class ServiceRegistry
    {
    public:
        template<typename TInterface>
        void registerService(TInterface* service)
        {
            m_services[std::type_index(typeid(TInterface))] = static_cast<void*>(service);
        }

        template<typename TInterface>
        TInterface* resolve() const
        {
            auto it = m_services.find(std::type_index(typeid(TInterface)));
            if (it == m_services.end())
                return nullptr;
            return static_cast<TInterface*>(it->second);
        }

        template<typename TInterface>
        bool isRegistered() const
        {
            return m_services.find(std::type_index(typeid(TInterface))) != m_services.end();
        }

    private:
        std::unordered_map<std::type_index, void*> m_services;
    };

} // namespace core
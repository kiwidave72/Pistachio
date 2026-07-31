#pragma once

// -----------------------------------------------------------------------
// IEventBus.h
//
// Lets plugins (service or UI) publish/subscribe to application events
// without holding direct references to each other — important once
// services and UI plugins are separate, independently hot-reloadable
// DLLs, since a direct callback into another plugin's code becomes a
// dangling-pointer risk the moment that plugin reloads.
//
// v1 payloads are strings by design: data-model keys/ids, or plain
// error/warning messages. This avoids raw struct pointers crossing the
// DLL boundary, which risks layout mismatches across independently
// compiled/hot-reloaded DLLs. Richer payloads can be reconsidered later
// once this pattern is proven.
//
// Both this (broadcast/notification) and direct service-to-service calls
// via ServiceRegistry::resolve<T>() (request/response) are allowed side
// by side — no pattern is being forced yet.
// -----------------------------------------------------------------------

#include <string>
#include <functional>

namespace ports {

    using EventHandler = std::function<void(const std::string& payload)>;

    // Opaque handle returned by subscribe(), usable to unsubscribe later.
    using SubscriptionId = uint64_t;

    class IEventBus
    {
    public:
        virtual ~IEventBus() = default;

        virtual void publish(const std::string& eventKey, const std::string& payload) = 0;

        virtual SubscriptionId subscribe(const std::string& eventKey, EventHandler handler) = 0;

        virtual void unsubscribe(const std::string& eventKey, SubscriptionId id) = 0;
    };

} // namespace core
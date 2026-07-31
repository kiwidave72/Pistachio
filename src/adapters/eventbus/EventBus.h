#pragma once

#include "ports/IEventBus.h"

#include <unordered_map>
#include <vector>
#include <mutex>

 

namespace adapters {
    
    class EventBus final : public ports::IEventBus
    {
    public:
        void publish(const std::string& eventKey, const std::string& payload) override;

        ports::SubscriptionId subscribe(const std::string& eventKey, ports::EventHandler handler) override;

        void unsubscribe(const std::string& eventKey, ports::SubscriptionId id) override;

    private:
        struct Subscription
        {
            ports::SubscriptionId id;
            ports::EventHandler handler;
        };

        std::mutex m_mutex;
        std::unordered_map<std::string, std::vector<Subscription>> m_subscriptions;
        ports::SubscriptionId m_nextId = 1;
    };

} // namespace core
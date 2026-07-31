#include "adapters/eventbus/EventBus.h"
#include <algorithm>

namespace adapters{
    
    void EventBus::publish(const std::string& eventKey, const std::string& payload)
    {
        // Copy the subscriber list out under the lock, then invoke handlers
        // without holding it — a handler is free to subscribe/unsubscribe/
        // publish in response without deadlocking.
        std::vector<Subscription> subscribersCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_subscriptions.find(eventKey);
            if (it == m_subscriptions.end())
                return;
            subscribersCopy = it->second;
        }

        for (auto& sub : subscribersCopy)
        {
            if (sub.handler)
                sub.handler(payload);
        }
    }

    ports::SubscriptionId EventBus::subscribe(const std::string& eventKey, ports::EventHandler handler)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ports::SubscriptionId id = m_nextId++;
        m_subscriptions[eventKey].push_back(Subscription{ id, std::move(handler) });
        return id;
    }

    void EventBus::unsubscribe(const std::string& eventKey, ports::SubscriptionId id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscriptions.find(eventKey);
        if (it == m_subscriptions.end())
            return;

        auto& subs = it->second;
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [id](const Subscription& s) { return s.id == id; }),
            subs.end());
    }

} // namespace core
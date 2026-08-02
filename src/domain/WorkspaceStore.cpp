#include "domain/WorkspaceStore.h"

namespace domain::v1 {

    WorkspaceStore::WorkspaceStore(ports::IEventBus* eventBus)
        : m_eventBus(eventBus)
    {
        // Matches DataContext's existing behavior — a Workspace instance
        // always exists from construction, never null. loadWorkspace()
        // deserializes into it in place rather than replacing it.
        m_workspace = std::make_unique<Workspace>();
    }

    WorkspaceStore::~WorkspaceStore() = default;

    void WorkspaceStore::read(const std::function<void(const Workspace&)>& fn) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (fn)
            fn(*m_workspace);
    }

    void WorkspaceStore::commit(const std::function<void(Workspace&)>& fn)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (fn)
                fn(*m_workspace);
        }

        // Published outside the lock, deliberately — a subscriber
        // reacting to this event might itself call read()/commit(),
        // which would deadlock if we were still holding m_mutex here.
        if (m_eventBus)
            m_eventBus->publish("workspace.changed", "");
    }

} // namespace domain::v1
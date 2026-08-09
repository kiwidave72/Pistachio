#pragma once

// -----------------------------------------------------------------------
// ToolpathStore.h
//
// Service-registered, in-process store for the current Toolpath — same
// spirit as WorkspaceStore, but simpler: Toolpath is always regenerated
// WHOLESALE by the pipeline (one complete replace per run), never
// incrementally mutated, so a locked set()/get() fits better than
// WorkspaceStore's mutate-in-place commit() gate.
//
// Eliminates the file-round-trip handoff between ToolpathEnginePlugin
// and ToolpathVisualizationPlugin for the live-pipeline case — set()
// publishes "toolpath.updated" via IEventBus; subscribers call get()
// directly, no file path needed. The file save/load path
// (ToolpathSerialization.h) stays useful for the separate offline
// dev-iteration workflow, not replaced by this.
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"
#include "ports/IEventBus.h"

#include <mutex>
#include <optional>

namespace domain::v1 {

    class ToolpathStore
    {
    public:
        explicit ToolpathStore(ports::IEventBus* eventBus = nullptr)
            : m_eventBus(eventBus) {
        }

        void set(Toolpath toolpath)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_toolpath = std::move(toolpath);
            }
            // Published outside the lock — same reasoning as WorkspaceStore::commit():
            // a subscriber reacting to this event might itself call get(),
            // which would deadlock if the lock were still held here.
            if (m_eventBus)
                m_eventBus->publish("toolpath.updated", "");
        }

        // Returns a copy — safe, simple, matches the "wholesale replace"
        // usage pattern. Toolpath sizes here (hundreds of thousands of
        // segments) make the copy non-trivial but acceptable for an
        // infrequent (once-per-pipeline-run) operation, not a per-frame one.
        std::optional<Toolpath> get() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_toolpath;
        }

        bool hasToolpath() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_toolpath.has_value();
        }

    private:
        mutable std::mutex m_mutex;
        std::optional<Toolpath> m_toolpath;
        ports::IEventBus* m_eventBus = nullptr;
    };

} // namespace domain::v1
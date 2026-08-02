#pragma once

// -----------------------------------------------------------------------
// WorkspaceStore.h
//
// The single controlled path to Workspace state. Formalizes a pattern
// SnapshotCommand was already doing informally (snapshot -> mutate ->
// snapshot) by giving it one real choke point, with locking and a
// change-notification event built in.
//
// - read()   — read-only access, no notification fired.
// - commit() — the mutation gate. Runs fn under the lock, then publishes
//              "workspace.changed" via IEventBus so anything holding
//              stale ids/pointers (NavigationManager, selection, etc.)
//              knows to re-validate rather than trust a pointer across
//              a commit — this is what closes the undo-dangling-pointer
//              bug from earlier work.
//
// Phase 1 of the WorkspaceStore design: this class is built and
// registered as a service, but nothing yet reads/writes through it.
// Existing direct dataContext.m_workspace access (~26 call sites,
// mostly in SlicerCorePlugin.cpp) is migrated later, deliberately as a
// separate, dedicated pass — not part of this change.
// -----------------------------------------------------------------------

#include "Workspace.h"
#include "ports/IEventBus.h"

#include <functional>
#include <memory>
#include <mutex>

namespace domain::v1 {

    class WorkspaceStore
    {
    public:
        // eventBus may be nullptr (e.g. in a minimal test harness) —
        // commit() simply skips publishing if so.
        explicit WorkspaceStore(ports::IEventBus* eventBus = nullptr);
        ~WorkspaceStore();

        WorkspaceStore(const WorkspaceStore&) = delete;
        WorkspaceStore& operator=(const WorkspaceStore&) = delete;

        void read(const std::function<void(const Workspace&)>& fn) const;

        void commit(const std::function<void(Workspace&)>& fn);

        // Escape hatch for code that isn't migrated yet (e.g. legacy
        // serialization call sites during the transition). Prefer
        // read()/commit() for anything new. Not locked — callers using
        // this directly are responsible for their own safety, same as
        // today's raw Workspace* access.
        Workspace& unsafeGet() { return *m_workspace; }
        const Workspace& unsafeGet() const { return *m_workspace; }

    private:
        mutable std::mutex m_mutex;
        std::unique_ptr<Workspace> m_workspace;
        ports::IEventBus* m_eventBus = nullptr;
    };

} // namespace domain::v1
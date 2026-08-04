#include "adapters/console/TaskProgressReporterConsole.h"

#include <cstdio>
#include <unordered_set>

namespace adapters::console {

    void TaskProgressReporterConsole::display()
    {
        m_runner.pumpCallbacks();

        auto groups = m_runner.snapshotGroups();

        // Drop tracking for any group no longer present (finished and
        // pruned by TaskRunner::pruneFinished()) — avoids m_lastPrinted
        // growing unboundedly over a long session, and avoids a stale
        // entry keyed by a pointer a later allocation could reuse.
        std::unordered_set<const TaskGroup*> current;
        for (auto& g : groups)
            current.insert(g.get());

        for (auto it = m_lastPrinted.begin(); it != m_lastPrinted.end(); )
        {
            if (current.find(it->first) == current.end())
                it = m_lastPrinted.erase(it);
            else
                ++it;
        }

        for (auto& gPtr : groups)
        {
            const TaskGroup& g = *gPtr;

            bool done = g.isDone();
            bool failed = g.isFailed();
            bool running = g.isRunning();
            bool pending = !g.started.load();

            float frac = g.aggregateFraction();
            int percent = (frac >= 0.f) ? static_cast<int>(frac * 100.f + 0.5f) : -1;

            std::string status = TaskRunner::badgeText(running, done, failed, pending);
            if (g.steps.size() > 1)
                status += " " + std::to_string(g.finishedCount()) + "/" + std::to_string((int)g.steps.size());

            // Fold in the most recent non-empty step message, if any, so a
            // message-only update (percent unchanged) still triggers a print.
            std::string stepMsg;
            for (auto& s : g.steps)
            {
                if (!s.progress) continue;
                std::string m = s.progress->getMessage();
                if (!m.empty()) stepMsg = m; // last non-empty wins
            }
            if (!stepMsg.empty())
                status += " - " + stepMsg;

            auto& last = m_lastPrinted[&g];
            if (last.percent == percent && last.message == status)
                continue; // nothing changed since last display() — stay silent

            if (percent >= 0)
                printf("[TaskRunner] %s: %d%% %s\n", g.label.c_str(), percent, status.c_str());
            else
                printf("[TaskRunner] %s: %s\n", g.label.c_str(), status.c_str());

            last.percent = percent;
            last.message = status;
        }
    }

} // namespace adapters::console
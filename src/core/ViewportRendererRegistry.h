#pragma once

// -----------------------------------------------------------------------
// ViewportRendererRegistry.h
//
// Concrete implementation of IViewportRendererRegistry. Plain
// unordered_map keyed by id — no locking. Registration/unregistration
// happens at plugin load/unload time (rare, not per-frame); render()
// calls read via resolve() every frame, but that's just a map lookup,
// no mutation during steady-state rendering. Same "add thread-safety
// once it's actually needed" discipline as ModelCache's own early phase.
// -----------------------------------------------------------------------

#include "ports/IViewportRendererRegistry.h"

#include <unordered_map>

namespace core {

    class ViewportRendererRegistry final : public ports::IViewportRendererRegistry
    {
    public:
        void registerRenderer(const std::string& id, I3DViewportGLRender* renderer) override
        {
            m_renderers[id] = renderer;
        }

        void unregisterRenderer(const std::string& id) override
        {
            m_renderers.erase(id);
        }

        std::vector<std::string> listRegisteredIds() const override
        {
            std::vector<std::string> ids;
            ids.reserve(m_renderers.size());
            for (auto& [id, renderer] : m_renderers)
                ids.push_back(id);
            return ids;
        }

        I3DViewportGLRender* resolve(const std::string& id) const override
        {
            auto it = m_renderers.find(id);
            return (it != m_renderers.end()) ? it->second : nullptr;
        }

    private:
        std::unordered_map<std::string, I3DViewportGLRender*> m_renderers;
    };

} // namespace core
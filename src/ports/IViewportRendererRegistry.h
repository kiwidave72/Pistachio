#pragma once

// -----------------------------------------------------------------------
// IViewportRendererRegistry.h
//
// Discovery mechanism for I3DViewportGLRender implementations. Same
// lifecycle discipline as IContributionRegistry: any plugin registers
// its own renderer at onLoad(), unregisters at onUnload(), the host UI
// never knows implementers in advance — it just lists whatever's
// currently registered and lets the user switch.
//
// Genuinely different shape from ServiceRegistry::resolve<T>() (one
// thing per type) — this is discovery of potentially many independent
// renderers, keyed by string id, not a single-instance resolve.
// -----------------------------------------------------------------------

#include <string>
#include <vector>

class I3DViewportGLRender;   // forward declaration — this header doesn't
// need the full interface definition, keeps
// the dependency minimal

namespace ports {

    class IViewportRendererRegistry
    {
    public:
        virtual ~IViewportRendererRegistry() = default;

        // Registration is idempotent per id — a second register() with
        // the same id overwrites the previous entry (matches
        // IConfigPort's own stated idempotent-registration convention).
        virtual void registerRenderer(const std::string& id, I3DViewportGLRender* renderer) = 0;

        // Safe to call even if id was never registered — no-op, not an
        // error. Plugins call this from onUnload() unconditionally.
        virtual void unregisterRenderer(const std::string& id) = 0;

        virtual std::vector<std::string> listRegisteredIds() const = 0;

        // Returns nullptr if id isn't currently registered — callers
        // (ViewportController::setActiveRenderer) must handle this,
        // not assume every id resolves.
        virtual I3DViewportGLRender* resolve(const std::string& id) const = 0;
    };

} // namespace ports
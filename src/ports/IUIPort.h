
#pragma once
#include <functional>
#include <string>

namespace ports {

// Keep the field names your Application.cpp is already using: id/name/version/featureGroup.
// Add extra diagnostics without breaking callers.
struct UiPluginStatus {
    bool enabled = true;
    bool loaded = false;

    std::string lastError;
    std::string sourcePath;
    std::string loadedPath;

    // Manifest (names expected by your core)
    std::string id;
    std::string name;
    std::string version;
    std::string featureGroup;
};

struct IUIPort {
    virtual ~IUIPort() = default;

    // ----- Existing UI lifecycle that your core::Application expects -----
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() const = 0;
    virtual void beginFrame() = 0;
    virtual void render() = 0;
    virtual void endFrame() = 0;
    virtual void setMenubarCallback(const std::function<void()>& menubarCallback) = 0;
    virtual void setRibbonbarCallback(const std::function<void()>& ribbonbarCallback) = 0;

    // ----- Plugin manager controls (new) -----
    virtual UiPluginStatus getUiPluginStatus() const = 0;
    virtual void setUiPluginEnabled(bool enabled) = 0;
    virtual void requestHotReloadUiPlugin() = 0;
};

} // namespace ports

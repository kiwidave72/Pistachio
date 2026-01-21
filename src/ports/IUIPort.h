#pragma once
#include <functional>

namespace ports {

class IUIPort {
public:
    virtual ~IUIPort() = default;
    
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void render() = 0;
    virtual void setMenubarCallback(const std::function<void()>& menubarCallback) = 0;
    // Hot reload the UI plugin module, if the current UI adapter supports it.
    // Returns true on success; false if unsupported or reload failed.
    virtual bool hotReloadUiPlugin() = 0;

};

} // namespace ports
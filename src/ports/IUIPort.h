#pragma once

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
};

} // namespace ports
#pragma once
#include <cstdint>

struct ImDrawList;

namespace ports {

struct Vec2 { float x = 0.0f; float y = 0.0f; };


struct ViewportRect {
    Vec2 pos;
    Vec2 size;
};

struct ViewportCamera {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float dist = 3.0f;

    float fovYRadians = 0.75f; // ~43 degrees
    float nearPlane = 0.05f;
    float farPlane  = 100.0f;
};

struct ViewportRenderContext {
    ImDrawList* backgroundDrawList = nullptr;
    ImDrawList* foregroundDrawList = nullptr;
    float deltaSeconds = 0.0f;
    bool canControl = false; // host sets this based on ImGui IO capture flags
};

class IViewportRendererPort {
public:
    virtual ~IViewportRendererPort() = default;
    virtual const char* name() const = 0;

    virtual void render(const ViewportRect& vp,
                        const ViewportCamera& cam,
                        const ViewportRenderContext& ctx) = 0;
};

} // namespace ports

#pragma once

// -----------------------------------------------------------------------
// I3DViewportGLRender.h
//
// Any plugin implementing this and registering itself with
// IViewportRendererRegistry becomes selectable in the viewport switcher.
// getTexture() returns a real GLuint � decided, not deferred (see design
// doc): this interface commits to GL, honestly reflected in its name.
//
// render() receives the current CameraState as a parameter every frame
// � implementations never own or mutate camera state themselves. This
// is what lets ViewportController own exactly one shared camera across
// however many registered renderers exist.
// -----------------------------------------------------------------------

#include "imgui.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>

#include "domain/RaycastHit.h"

using GLuint = unsigned int;   // avoid pulling a full GL header into this
// port � matches the value GLAD/GL headers
// define, safe as a forward-compatible typedef

struct CameraState
{
    glm::vec3 target;
    float distance;
    float yaw;
    float pitch;
    float fovYRadians = 0.75f;
    float nearPlane = 0.05f;
    float farPlane = 2000.0f;

    // Diagnostic only -- see the long comment on ViewportController::
    // cycleCameraAxisMode(). A field here (not a static in a header
    // function) because CameraState is a plain struct passed by reference
    // from the one shared ViewportController instance into whichever
    // renderer is active each frame, so it's genuinely shared across every
    // plugin DLL -- a function-local static in an inline header function
    // is NOT: each DLL that includes it gets its own independent copy.
    // Mode 0 is the confirmed-correct convention (see buildCameraContext()
    // in RenderCameraContext.h); default here is explicitly set to 1
    // ("Un-flipped (pre-fix)") per request, not 0.
    int axisMode = 0;
};

struct ViewportRenderContext
{
    bool canControl = false;
    float deltaSeconds = 0.0f;
};


class I3DViewportGLRender
{
public:
    virtual ~I3DViewportGLRender() = default;


    virtual void setVisibleLayerRange(int startLayer, int endLayer) {}
    virtual int layerCount() const { return 0; }
    virtual int visibleLayer() const { return -1; }
    virtual void setVisibleLayer(int layer) {}

    virtual glm::vec3 defaultTarget() const { return glm::vec3(0.0f); }
    virtual float defaultDistance() const { return 200.0f; }
    virtual float defaultYaw() const { return 0.0f; }
    virtual float defaultPitch() const { return glm::radians(45.0f); }

    virtual const char* name() const = 0;

    virtual void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) = 0;
    virtual void renderUI(ImVec2 topLeft, ImVec2 size) {}

    virtual GLuint getTexture() const = 0;
    virtual RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera) = 0;
};
#pragma once

// -----------------------------------------------------------------------
// I3DViewportGLRender.h
//
// Any plugin implementing this and registering itself with
// IViewportRendererRegistry becomes selectable in the viewport switcher.
// getTexture() returns a real GLuint — decided, not deferred (see design
// doc): this interface commits to GL, honestly reflected in its name.
//
// render() receives the current CameraState as a parameter every frame
// — implementations never own or mutate camera state themselves. This
// is what lets ViewportController own exactly one shared camera across
// however many registered renderers exist.
// -----------------------------------------------------------------------

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
 
using GLuint = unsigned int;   // avoid pulling a full GL header into this
// port — matches the value GLAD/GL headers
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


};

struct ViewportRenderContext
{
    bool canControl = false;
    float deltaSeconds = 0.0f;
};

struct RaycastHit
{
    bool hit = false;
    glm::vec3 position{ 0.0f };
    std::string objectId;   // empty if nothing hit — identity of whatever was struck, meaning is renderer-specific
};

class I3DViewportGLRender
{
public:
    virtual ~I3DViewportGLRender() = default;


    virtual int layerCount() const { return 0; }
    virtual int visibleLayer() const { return -1; }
    virtual void setVisibleLayer(int layer) {}

    virtual const char* name() const = 0;

    virtual void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) = 0;
    virtual GLuint getTexture() const = 0;
    virtual RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera) = 0;
};
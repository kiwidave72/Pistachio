#pragma once

// -----------------------------------------------------------------------
// ViewportController.h
//
// Owned directly by SlicerCorePlugin as a plain member — NOT a service,
// NOT registered anywhere. Exactly one instance, ever. Owns the single
// shared CameraState/orbit math, which registered renderer is currently
// active, and the camera gizmo (fixed chrome, not a swappable renderer
// — owned directly, not through IViewportRendererRegistry).
// -----------------------------------------------------------------------

#include "ports/I3DViewportGLRender.h"
#include "ports/IViewportRendererRegistry.h"
#include "adapters/rendering/CameraGizmoGLRender.h"

#include <memory>
#include <string>

class ViewportController
{
public:
    explicit ViewportController(ports::IViewportRendererRegistry& registry)
        : m_registry(registry)
    {
        m_camera.target = glm::vec3(0.0f);
        m_camera.distance = 200.0f;
        m_camera.yaw = 4.71239f;              // matches SceneLayout::m_defaultYaw (already radians)
        m_camera.pitch = glm::radians(45.0f);  // matches SceneLayout::m_defaultPitch (converted from degrees)

        m_gizmo = std::make_unique<CameraGizmoGLRender>();
    }

    void setActiveRenderer(const std::string& id)
    {
        m_activeRenderer = m_registry.resolve(id);
        m_activeId = m_activeRenderer ? id : std::string();
    }

    const std::string& activeId() const { return m_activeId; }
    bool hasActiveRenderer() const { return m_activeRenderer != nullptr; }

    void orbit(float deltaYaw, float deltaPitch)
    {
        m_camera.yaw += deltaYaw;
        m_camera.pitch = glm::clamp(m_camera.pitch + deltaPitch, -1.5f, 1.5f);
    }

    void zoom(float delta)
    {
        m_camera.distance = glm::clamp(m_camera.distance - delta, 5.0f, 2000.0f);
    }

    void setTarget(const glm::vec3& target) { m_camera.target = target; }

    // Absolute set — used by the gizmo's click-to-snap, distinct from
    // orbit()'s incremental delta used for drag.
    void setYawPitch(float yaw, float pitch)
    {
        m_camera.yaw = yaw;
        m_camera.pitch = glm::clamp(pitch, -1.5f, 1.5f);
    }

    const CameraState& camera() const { return m_camera; }

    GLuint renderAndGetTexture(uint32_t width, uint32_t height, float deltaSeconds, bool canControl)
    {
        if (!m_activeRenderer) return 0;

        ViewportRenderContext ctx;
        ctx.canControl = canControl;
        ctx.deltaSeconds = deltaSeconds;

        m_activeRenderer->render(width, height, m_camera, ctx);
        return m_activeRenderer->getTexture();
    }

    // Renders the gizmo into its own small FBO, sized independently of
    // the main viewport. Caller displays it as a separate small
    // ImGui::Image() overlay in the viewport corner.
    GLuint renderGizmoAndGetTexture(uint32_t gizmoSize)
    {
        m_gizmo->render(gizmoSize, gizmoSize, m_camera);
        return m_gizmo->getTexture();
    }

    // handleGizmoClick() — needs gizmoSize passed through, matching the new hitTestFace signature
    bool handleGizmoClick(float gizmoLocalX, float gizmoLocalY, uint32_t gizmoSize)
    {
        float yaw, pitch;
        if (m_gizmo->hitTestFace(gizmoLocalX, gizmoLocalY, gizmoSize, gizmoSize, yaw, pitch))
        {
            setYawPitch(yaw, pitch);
            return true;
        }
        return false;
    }

    
    CameraGizmoGLRender* gizmo() const { return m_gizmo.get(); }

    int activeLayerCount() const { return m_activeRenderer ? m_activeRenderer->layerCount() : 0; }
    int activeVisibleLayer() const { return m_activeRenderer ? m_activeRenderer->visibleLayer() : -1; }
    void setActiveVisibleLayer(int layer) { if (m_activeRenderer) m_activeRenderer->setVisibleLayer(layer); }

private:
    ports::IViewportRendererRegistry& m_registry;
    I3DViewportGLRender* m_activeRenderer = nullptr;
    std::string m_activeId;
    CameraState m_camera;


    std::unique_ptr<CameraGizmoGLRender> m_gizmo;
};
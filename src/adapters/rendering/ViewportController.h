#pragma once

// -----------------------------------------------------------------------
// ViewportController.h
//
// Owned directly by SlicerCorePlugin as a plain member � NOT a service,
// NOT registered anywhere. Exactly one instance, ever. Owns the single
// shared CameraState/orbit math, which registered renderer is currently
// active, and the camera gizmo (fixed chrome, not a swappable renderer
// � owned directly, not through IViewportRendererRegistry).
// -----------------------------------------------------------------------

#include "ports/I3DViewportGLRender.h"
#include "ports/IViewportRendererRegistry.h"
#include "adapters/rendering/CameraGizmoGLRender.h"
#include "domain/RenderCameraContext.h"
#include "core/IEasedTransition.h"
#include "core/CubicEasedTransition.h"

#include <glm/gtc/constants.hpp>
#include <memory>
#include <string>
#include <cmath>

class ViewportController
{
public:
    explicit ViewportController(ports::IViewportRendererRegistry& registry)
        : m_registry(registry)
    {
        m_camera.target = glm::vec3(0.0f);
        m_camera.distance = 200.0f;
        m_camera.yaw = 0.0f;// 4.71239f;              // matches SceneLayout::m_defaultYaw (already radians)
        m_camera.pitch = glm::radians(35.0f);  // matches SceneLayout::m_defaultPitch (converted from degrees)

        m_gizmo = std::make_unique<CameraGizmoGLRender>();
    }

    void setActiveRenderer(const std::string& id)
    {
        // m_camera is intentionally left untouched here -- it's one
        // shared camera across every registered renderer (see the class
        // comment), so switching renderers should not reset it to that
        // renderer's default*() values. Whatever orbit/zoom state the
        // user was at carries straight over.
        m_activeRenderer = m_registry.resolve(id);
        m_activeId = m_activeRenderer ? id : std::string();
    }

    const std::string& activeId() const { return m_activeId; }
    bool hasActiveRenderer() const { return m_activeRenderer != nullptr; }

    void orbit(float deltaYaw, float deltaPitch)
    {
        cancelCameraTransition();   // manual input interrupts any in-flight animateTo()
        m_camera.yaw += deltaYaw;
        m_camera.pitch = glm::clamp(m_camera.pitch + deltaPitch, -1.5f, 1.5f);
    }

    void zoom(float delta)
    {
        cancelCameraTransition();
        m_camera.distance = glm::clamp(m_camera.distance - delta, 5.0f, 2000.0f);
    }

    // Screen-space pan: shifts the orbit target along the camera's own
    // right/up vectors (same right/up formulas as buildCameraContext()'s
    // confirmed-correct mode 0), scaled by distance so the drag feels
    // consistent whether zoomed in close or far out. camPos is re-derived
    // every frame from target+yaw+pitch+distance, so moving target alone
    // is enough -- no separate camPos to keep in sync.
    void pan(float dxPixels, float dyPixels)
    {
        cancelCameraTransition();
        glm::vec3 camPos(
            m_camera.target.x + m_camera.distance * std::cos(m_camera.pitch) * std::cos(m_camera.yaw),
            m_camera.target.y + m_camera.distance * std::sin(m_camera.pitch),
            m_camera.target.z + m_camera.distance * std::cos(m_camera.pitch) * std::sin(m_camera.yaw));

        glm::vec3 forward = glm::normalize(m_camera.target - camPos);
        glm::vec3 right(std::sin(m_camera.yaw), 0.0f, -std::cos(m_camera.yaw));
        glm::vec3 up = -glm::normalize(glm::cross(right, forward));

        float scale = m_camera.distance * 0.0015f;
        m_camera.target += (-right * dxPixels - up * dyPixels) * scale;
    }

    // Dolly zoom that pulls the orbit pivot toward the point under the
    // cursor while zooming IN, so the view visibly converges onto
    // whatever you're pointed at over several scroll ticks -- each tick
    // blends only a fraction of the remaining distance (proportional to
    // how much closer this tick just brought you), so it reads as a
    // smooth homing-in rather than a jump, and lands exactly on the
    // point once fully zoomed in. Zooming back OUT leaves the pivot
    // alone -- pulling it toward hitPoint on the way out as well would
    // make the view lurch sideways as you pull back. hitPoint is
    // nullptr when the cursor isn't over any geometry, in which case
    // this is just a plain dolly around the existing target.
    void zoomToPoint(float delta, const glm::vec3* hitPoint)
    {
        cancelCameraTransition();
        float oldDistance = m_camera.distance;
        float newDistance = glm::clamp(oldDistance - delta, 5.0f, 2000.0f);
        m_camera.distance = newDistance;

        if (hitPoint && delta > 0.0f && oldDistance > 0.01f)
        {
            float t = glm::clamp(1.0f - (newDistance / oldDistance), 0.0f, 1.0f);
            m_camera.target = glm::mix(m_camera.target, *hitPoint, t);
        }
    }

    // Casts a ray against whichever renderer is currently active. Returns
    // a default (RaycastHit::hit == false) if there's no active renderer.
    RaycastHit raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection)
    {
        return m_activeRenderer ? m_activeRenderer->raycast(rayOrigin, rayDirection, m_camera) : RaycastHit{};
    }

    void setActiveVisibleLayerRange(int start, int end) { if (m_activeRenderer) m_activeRenderer->setVisibleLayerRange(start, end); }

    void setTarget(const glm::vec3& target) { cancelCameraTransition(); m_camera.target = target; }

    // Absolute set � used by the gizmo's click-to-snap, distinct from
    // orbit()'s incremental delta used for drag.
    void setYawPitch(float yaw, float pitch)
    {
        cancelCameraTransition();
        m_camera.yaw = yaw;
        m_camera.pitch = glm::clamp(pitch, -1.5f, 1.5f);
    }

    const CameraState& camera() const { return m_camera; }

    // -----------------------------------------------------------------
    // Eased camera transition -- animates the one shared CameraState from
    // wherever it currently is toward `target` over durationSeconds, using
    // the same CubicEasedTransition/IEasedTransition policy AnimatedModel
    // uses for per-model transforms (see core/CubicEasedTransition.h).
    // Lives here rather than in any one renderer because the camera is
    // shared across every registered renderer (see the class comment) --
    // e.g. the multi-plate <-> single-plate view toggle animates this
    // same camera regardless of which renderer ends up active.
    //
    // Only target/distance/yaw/pitch are eased; axisMode/fov/near/far
    // carry over from whatever the camera is currently set to, untouched.
    // Any manual camera input (orbit/pan/zoom/zoomToPoint/setTarget/
    // setYawPitch) cancels an in-flight transition immediately -- the user
    // taking control should never fight an animation still resolving.
    // -----------------------------------------------------------------
    void animateTo(const CameraState& target, float durationSeconds)
    {
        m_cameraTransitionFrom = m_camera;
        m_cameraTransitionTo = target;
        m_cameraTransitionTo.axisMode = m_camera.axisMode;
        m_cameraTransitionTo.fovYRadians = m_camera.fovYRadians;
        m_cameraTransitionTo.nearPlane = m_camera.nearPlane;
        m_cameraTransitionTo.farPlane = m_camera.farPlane;
        m_cameraTransition->start(durationSeconds);
        m_cameraTransitionActive = true;
    }

    bool isCameraAnimating() const { return m_cameraTransitionActive; }

    // Sets the camera directly to `target`, with no easing -- for
    // callers that want an instant cut instead of animateTo()'s eased
    // transition (e.g. rapid-fire plate cycling on repeated key presses,
    // where a per-step animation would fight itself and feel laggy).
    // Cancels any in-flight transition. Only target/distance/yaw/pitch
    // come from `target`; axisMode/fov/near/far carry over from the
    // current camera untouched, same convention as animateTo().
    void setCameraImmediate(const CameraState& target)
    {
        cancelCameraTransition();
        float fov = m_camera.fovYRadians, nearP = m_camera.nearPlane, farP = m_camera.farPlane;
        int axisMode = m_camera.axisMode;
        m_camera = target;
        m_camera.fovYRadians = fov;
        m_camera.nearPlane = nearP;
        m_camera.farPlane = farP;
        m_camera.axisMode = axisMode;
    }

    // Diagnostic only -- see the long comment on CameraState::axisMode in
    // ports/I3DViewportGLRender.h. Mutates the ONE shared CameraState
    // instance (m_camera below), which every renderer -- regardless of
    // which plugin DLL it lives in -- receives by reference each frame via
    // renderAndGetTexture()'s render() call. This is what makes the toggle
    // genuinely affect every view at once; a static in a header function
    // could not, since each DLL gets its own independent copy of that.
    void cycleCameraAxisMode() { m_camera.axisMode = (m_camera.axisMode + 1) % 4; }
    const char* cameraAxisModeName() const { return domain::v1::cameraAxisModeName(m_camera.axisMode); }

    GLuint renderAndGetTexture(uint32_t width, uint32_t height, float deltaSeconds, bool canControl)
    {
        tickCameraTransition(deltaSeconds);

        if (!m_activeRenderer) return 0;

        ViewportRenderContext ctx;
        ctx.canControl = canControl;
        ctx.deltaSeconds = deltaSeconds;

        m_activeRenderer->render(width, height, m_camera, ctx);
        return m_activeRenderer->getTexture();
    }
    void renderActiveUI(ImVec2 topLeft, ImVec2 size)
    {
        if (m_activeRenderer)
            m_activeRenderer->renderUI(topLeft, size);
    }
    // Renders the gizmo into its own small FBO, sized independently of
    // the main viewport. Caller displays it as a separate small
    // ImGui::Image() overlay in the viewport corner.
    GLuint renderGizmoAndGetTexture(uint32_t gizmoSize)
    {
        m_gizmo->render(gizmoSize, gizmoSize, m_camera);
        return m_gizmo->getTexture();
    }

    // handleGizmoClick() � needs gizmoSize passed through, matching the new hitTestFace signature
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
    // Cancels an in-flight animateTo() without touching m_camera itself --
    // called from every manual camera-control entry point above so user
    // input always wins over a still-resolving transition.
    void cancelCameraTransition() { m_cameraTransitionActive = false; }

    // Advances the eased transition (if any) and writes the blended
    // result into m_camera. Shortest-path yaw lerp matches the identical
    // logic in the old (dead) SceneLayout::update() -- orbiting the long
    // way around on a >180 degree yaw change would look wrong otherwise.
    void tickCameraTransition(float deltaSeconds)
    {
        if (!m_cameraTransitionActive) return;

        m_cameraTransition->tick(deltaSeconds);
        float e = m_cameraTransition->progress();

        m_camera.target = glm::mix(m_cameraTransitionFrom.target, m_cameraTransitionTo.target, e);
        m_camera.distance = glm::mix(m_cameraTransitionFrom.distance, m_cameraTransitionTo.distance, e);
        m_camera.pitch = glm::mix(m_cameraTransitionFrom.pitch, m_cameraTransitionTo.pitch, e);

        float yawDelta = m_cameraTransitionTo.yaw - m_cameraTransitionFrom.yaw;
        if (yawDelta > glm::pi<float>()) yawDelta -= glm::two_pi<float>();
        if (yawDelta < -glm::pi<float>()) yawDelta += glm::two_pi<float>();
        m_camera.yaw = m_cameraTransitionFrom.yaw + yawDelta * e;

        if (!m_cameraTransition->isAnimating())
            m_cameraTransitionActive = false;
    }

    ports::IViewportRendererRegistry& m_registry;
    I3DViewportGLRender* m_activeRenderer = nullptr;
    std::string m_activeId;
    CameraState m_camera;

    bool m_cameraTransitionActive = false;
    CameraState m_cameraTransitionFrom{};
    CameraState m_cameraTransitionTo{};
    std::unique_ptr<core::IEasedTransition> m_cameraTransition = std::make_unique<core::CubicEasedTransition>();

    std::unique_ptr<CameraGizmoGLRender> m_gizmo;
};
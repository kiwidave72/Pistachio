#pragma once
#include "core/IEasedTransition.h"
#include "ports/IDrawable.h"
#include "domain/RenderModel.h"
#include "domain/RaycastHit.h"
#include <memory>

inline Transform lerpTransform(const Transform& a, const Transform& b, float t)
{
    Transform r;
    r.position = glm::mix(a.position, b.position, t);
    r.rotation = glm::mix(a.rotation, b.rotation, t);
    r.scale = glm::mix(a.scale, b.scale, t);
    return r;
}
inline glm::vec3 lerpColor(glm::vec3 a, glm::vec3 b, float t) { return glm::mix(a, b, t); }

class AnimatedModel
{
public:
    AnimatedModel(std::shared_ptr<IDrawable> drawable, std::shared_ptr<IModelRenderStrategy> strategy,
        std::unique_ptr<core::IEasedTransition> transformTransition,
        std::unique_ptr<core::IEasedTransition> colorTransition)
        : m_drawable(std::move(drawable)), m_strategy(std::move(strategy)),
        m_transformTransition(std::move(transformTransition)), m_colorTransition(std::move(colorTransition)) {
    }

    void tick(float dt) { m_transformTransition->tick(dt); m_colorTransition->tick(dt); }

    void animateTransformTo(const Transform& target, float durationSeconds)
    {
        m_transformFrom = currentTransform(); m_transformTarget = target; m_transformTransition->start(durationSeconds);
    }
    void animateColorTo(glm::vec3 target, float durationSeconds)
    {
        m_colorFrom = currentColor(); m_colorTarget = target; m_colorTransition->start(durationSeconds);
    }
    void setImmediateTransform(const Transform& t) { m_transformFrom = m_transformTarget = t; }
    void setImmediateColor(glm::vec3 c) { m_colorFrom = m_colorTarget = c; }
    void setGhostFactor(float g) { m_ghostFactor = g; }

    void render(GLuint shader, const domain::v1::RenderCameraContext& cameraContext, glm::vec2 center)
    {
        Transform t = lerpTransform(m_transformFrom, m_transformTarget, m_transformTransition->progress());
        RenderDrawState ds{ lerpColor(m_colorFrom, m_colorTarget, m_colorTransition->progress()), m_ghostFactor };
        m_drawable->draw(*m_strategy, ds, cameraContext, t, shader, center);
    }

    // Raycasts against the underlying RenderModel using this animated
    // model's CURRENT (interpolated) transform, so hit-testing matches
    // what's actually on screen this frame -- not the transition target.
    // Returns false (no RenderModel found / no hit) without touching outHit.
    bool raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
        glm::vec2 center, RaycastHit& outHit, bool isPlate = false) const
    {
        auto* rm = dynamic_cast<RenderModel*>(m_drawable.get());
        if (!rm) return false;

        glm::mat4 modelMatrix = rm->getModelMatrix(currentTransform(), center, isPlate);
        if (!rm->raycastBoundsOnly(rayOrigin, rayDirection, modelMatrix)) return false;
        return rm->raycast(rayOrigin, rayDirection, outHit, modelMatrix);
    }

private:
    Transform currentTransform() const { return lerpTransform(m_transformFrom, m_transformTarget, m_transformTransition->progress()); }
    glm::vec3 currentColor() const { return lerpColor(m_colorFrom, m_colorTarget, m_colorTransition->progress()); }

    std::shared_ptr<IDrawable> m_drawable;
    std::shared_ptr<IModelRenderStrategy> m_strategy;
    Transform m_transformFrom, m_transformTarget;
    std::unique_ptr<core::IEasedTransition> m_transformTransition;
    glm::vec3 m_colorFrom{ 1.0f }, m_colorTarget{ 1.0f };
    std::unique_ptr<core::IEasedTransition> m_colorTransition;
    float m_ghostFactor = 1.0f;
};
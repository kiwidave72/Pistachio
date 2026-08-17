#pragma once
#include "ports/IModelRenderStrategy.h"
#include "domain/RenderCameraContext.h"

class IDrawable
{
public:
    virtual ~IDrawable() = default;
    virtual void draw(const IModelRenderStrategy& strategy, const RenderDrawState& drawState,
        const domain::v1::RenderCameraContext& cameraContext, const Transform& transform,
        GLuint shader, glm::vec2 center) = 0;
};
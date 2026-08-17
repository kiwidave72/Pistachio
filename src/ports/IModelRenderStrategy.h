#pragma once
#include "domain/Transform.h"
#include <glad/glad.h>
#include <glm/glm.hpp>

struct RenderDrawState { glm::vec3 color{ 1.0f }; float ghostFactor = 1.0f; };

class IModelRenderStrategy
{
public:
    virtual ~IModelRenderStrategy() = default;
    virtual glm::mat4 computeModelMatrix(const Transform& transform, glm::vec2 center) const = 0;
    virtual void applyShaderUniforms(GLuint shader, glm::vec3 color, float ghostFactor) const = 0;
};
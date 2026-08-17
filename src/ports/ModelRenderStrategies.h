#pragma once
#include "ports/IModelRenderStrategy.h"
#include "domain/AxisConvention.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    inline void applyStandardUniforms(GLuint shader, glm::vec3 color, float ghostFactor)
    {
        glUniform3f(glGetUniformLocation(shader, "uBaseColor"), color.r, color.g, color.b);
        glUniform1f(glGetUniformLocation(shader, "uGhostFactor"), ghostFactor);
    }
}

class StandardModelRenderStrategy final : public IModelRenderStrategy
{
public:
    glm::mat4 computeModelMatrix(const Transform& transform, glm::vec2 center) const override
    {
        // transform.position is domain-space (3D-printing convention: Z is
        // height above the bed, X/Y is the footprint plane), matching the
        // same convention mesh vertex data is authored in -- not the
        // engine's Y-up world space. Mesh vertices get converted between
        // the two via axisFix below (a -90 degree rotation about X, which
        // maps local (x,y,z) -> world (x,z,-y)); transform.position needs
        // that exact same conversion applied to it, or a domain Z (height)
        // value ends up being read as a world Y (also height, coincidentally
        // the same *axis*, but previously passed straight through
        // unconverted) while domain Y (footprint) was being passed straight
        // into world Z with the wrong sign -- both silently wrong in a way
        // that only shows up once height is nonzero. This was long masked
        // by every real position so far having Z (height) == 0.
        glm::vec3 worldPos(center.x + transform.position.x, transform.position.z, center.y - transform.position.y);
        glm::mat4 worldPosMat = glm::translate(glm::mat4(1.0f), worldPos);
        glm::mat4 axisFix = domain::v1::axisFixMatrix();
        glm::mat4 rotScale(1.0f);
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.x), glm::vec3(1, 0, 0));
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
        rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.z), glm::vec3(0, 0, 1));
        rotScale = glm::scale(rotScale, transform.scale);
        return worldPosMat * axisFix * rotScale;
    }
    void applyShaderUniforms(GLuint shader, glm::vec3 color, float ghostFactor) const override
    {
        applyStandardUniforms(shader, color, ghostFactor);
    }
};

class PlateModelRenderStrategy final : public IModelRenderStrategy
{
public:
    // The plate mesh's local Z range (its thickness, e.g. 0..8 for an
    // 8-unit-thick bed) maps straight to world Y via axisFix, with no
    // shift -- so its top surface sits at world Y=(mesh's own max Z), not
    // world Y=0. Instances resting on the bed (domain Z=0) correctly land
    // at world Y=0 via StandardModelRenderStrategy, so without this
    // offset they end up sunk into the plate by exactly its own
    // thickness. setHeightOffset(-meshBounds.max.z), called once by
    // whoever builds this strategy (see EditableSceneLayout::
    // setActiveBuildPlate), shifts the plate down so its top surface
    // lands at world Y=0 instead -- this doesn't move any instances, only
    // where the plate mesh itself renders.
    void setHeightOffset(float z) { m_heightOffset = z; }

    glm::mat4 computeModelMatrix(const Transform& transform, glm::vec2 center) const override
    {
        glm::vec3 worldPos(center.x + transform.position.x, m_heightOffset, center.y + transform.position.z);
        glm::mat4 worldPosMat = glm::translate(glm::mat4(1.0f), worldPos);
        return worldPosMat * domain::v1::axisFixMatrix();
    }
    void applyShaderUniforms(GLuint shader, glm::vec3 color, float ghostFactor) const override
    {
        applyStandardUniforms(shader, color, ghostFactor);
    }

private:
    float m_heightOffset = 0.0f;
};
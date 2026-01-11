#pragma once

#include <glm/glm.hpp>

namespace core::rendering
{
    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };

    struct CameraState
    {
        // World-space camera definition
        glm::vec3 position { 0.0f, 0.0f, 10.0f };
        glm::vec3 target   { 0.0f, 0.0f, 0.0f };
        glm::vec3 up       { 0.0f, 1.0f, 0.0f };

        ProjectionType projection = ProjectionType::Perspective;

        // Projection parameters
        float fovYDegrees = 45.0f;   // perspective
        float orthoScale  = 10.0f;   // orthographic half-height

        float nearPlane = 0.01f;
        float farPlane  = 10000.0f;

        // Viewport
        int viewportWidth  = 1;
        int viewportHeight = 1;
    };
}

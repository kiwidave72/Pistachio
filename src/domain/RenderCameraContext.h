#pragma once
#include "ports/I3DViewportGLRender.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace domain::v1 {

    struct RenderCameraContext { glm::mat4 view; glm::mat4 proj; glm::vec3 camPos; glm::vec3 target; };

    // Diagnostic mode names. Mode 0 ("Default") is the confirmed-correct
    // convention already baked into buildCameraContext() below -- the
    // other modes are exploratory variations relative to THAT baseline,
    // not relative to the original (broken) unflipped cross-product. This
    // way cycling through modes, or leaving the toggle on any value, can
    // never land back on the old broken behavior.
    inline const char* cameraAxisModeName(int mode)
    {
        switch (mode)
        {
        case 0: return "Default";
        case 1: return "Un-flipped (pre-fix)";
        case 2: return "Right flipped";
        case 3: return "Up<->Right swapped";
        }
        return "?";
    }

    inline RenderCameraContext buildCameraContext(const CameraState& camera, float aspect)
    {
        glm::vec3 camPos(
            camera.target.x + camera.distance * cos(camera.pitch) * cos(camera.yaw),
            camera.target.y + camera.distance * sin(camera.pitch),
            camera.target.z + camera.distance * cos(camera.pitch) * sin(camera.yaw));
        glm::vec3 right(sin(camera.yaw), 0.0f, -cos(camera.yaw));
        glm::vec3 forward = glm::normalize(camera.target - camPos);
        // Confirmed-correct baseline (mode 0): negated relative to the
        // naive cross(right, forward). Found empirically via the (now
        // restored, diagnostic-only) axis-mode toggle -- both the editable
        // scene and the debug visualiser only read correctly (letters
        // right-side-up and legible) with this flip applied.
        //
        // Verified again the hard way: removing this negation was tried
        // as a fix for a multi-plate-view raycast miss (hand-derived
        // math suggested it) -- it visibly flipped rendering everywhere
        // AND did not fix the raycast miss. Both outcomes confirm this
        // negation is correct and the raycast bug lives elsewhere; do
        // not remove it again without visual confirmation first.
        glm::vec3 up = -glm::normalize(glm::cross(right, forward));

        switch (camera.axisMode)
        {
        case 1: up = -up; break;                                          // back to the original unflipped cross-product, for comparison
        case 2: right = -right; break;
        case 3: { glm::vec3 tmp = right; right = up; up = tmp; break; }
        default: break;                                                   // mode 0: confirmed-correct baseline, unchanged
        }

        glm::mat4 view(
            right.x, up.x, -forward.x, 0.0f, right.y, up.y, -forward.y, 0.0f,
            right.z, up.z, -forward.z, 0.0f,
            -glm::dot(right, camPos), -glm::dot(up, camPos), glm::dot(forward, camPos), 1.0f);
        glm::mat4 proj = glm::perspective(camera.fovYRadians, aspect, camera.nearPlane, camera.farPlane);
        return { view, proj, camPos, camera.target };
    }

} // namespace domain::v1
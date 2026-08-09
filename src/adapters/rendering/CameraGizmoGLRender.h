#pragma once

#include "ports/I3DViewportGLRender.h"

#include <vector>
#include <string>

struct GizmoFace
{
    uint32_t idxOffset;
    uint32_t idxCount;
    glm::vec3 normal;
    glm::vec3 color;
    const char* label;
    float snapYaw;
    float snapPitch;
};

class CameraGizmoGLRender
{
public:
    CameraGizmoGLRender();
    ~CameraGizmoGLRender();

    void render(uint32_t width, uint32_t height, const CameraState& camera);
    GLuint getTexture() const;

    // Real ray-based hit test — same algorithm as BuildPlateRenderer's
    // renderCameraGizmo(), using the exact proj/view stored from the
    // most recent render() call. localX/localY are pixel coords within
    // the gizmo's own render area (0,0 = top-left).
    bool hitTestFace(float localX, float localY, uint32_t gizmoW, uint32_t gizmoH, float& outYaw, float& outPitch) const;

    // For the caller to draw labels/arrows with ImDrawList — this class
    // stays GL-only per naming convention, never touches ImGui itself.
    const std::vector<GizmoFace>& faces() const { return m_faces; }
    glm::vec3 cameraForward() const { return m_lastCamFwd; }
    glm::vec2 project2D(const glm::vec3& worldPos, uint32_t gizmoW, uint32_t gizmoH) const;

private:
    void ensureGl();
    void ensureFramebuffer(uint32_t width, uint32_t height);

    bool m_glInitialized = false;
    uint32_t m_fboWidth = 0;
    uint32_t m_fboHeight = 0;

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;

    std::vector<GizmoFace> m_faces;
    int m_hoveredFace = -1;

    // Stored from the most recent render() call, for hitTestFace()/
    // project2D() to reuse exactly — same matrices, same results.
    glm::mat4 m_lastView{ 1.0f };
    glm::mat4 m_lastProj{ 1.0f };
    glm::vec3 m_lastCamFwd{ 0.0f };
};
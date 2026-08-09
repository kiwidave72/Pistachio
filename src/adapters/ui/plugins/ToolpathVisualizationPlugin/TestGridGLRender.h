#pragma once

#include "ports/I3DViewportGLRender.h"

class TestGridGLRender final : public I3DViewportGLRender
{
public:
    TestGridGLRender();
    ~TestGridGLRender() override;

    const char* name() const override { return "Toolpath (test grid)"; }

    void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) override;
    GLuint getTexture() const override;
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera) override;

private:
    void ensureGl();
    void ensureFramebuffer(uint32_t width, uint32_t height);

    bool m_glInitialized = false;
    uint32_t m_fboWidth = 0;
    uint32_t m_fboHeight = 0;

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;

    GLuint m_lineShader = 0;
    GLuint m_gridVao = 0;
    GLuint m_gridVbo = 0;
    int m_gridVertexCount = 0;
};
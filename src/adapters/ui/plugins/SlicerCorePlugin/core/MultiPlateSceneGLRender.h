// MultiPlateSceneGLRender.h
#pragma once
#include "ports/I3DViewportGLRender.h"
#include "MultiPlateSceneLayout.h"

class MultiPlateSceneGLRender final : public I3DViewportGLRender
{
public:
    const char* name() const override { return "Multi-Plate Scene"; }
    void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) override;
    GLuint getTexture() const override;
    RaycastHit raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const CameraState&) override;

    MultiPlateSceneLayout& sceneLayout() { return m_sceneLayout; }

private:
    void ensureGl();
    void ensureFramebuffer(uint32_t w, uint32_t h);

    bool m_glInitialized = false;
    uint32_t m_fboWidth = 0, m_fboHeight = 0;
    GLuint m_fbo = 0, m_colorTexture = 0, m_depthRenderbuffer = 0;
    GLuint m_shader = 0;

    MultiPlateSceneLayout m_sceneLayout;
};
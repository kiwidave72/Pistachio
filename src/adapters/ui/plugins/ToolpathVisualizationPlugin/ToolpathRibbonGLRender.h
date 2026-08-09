#pragma once

// -----------------------------------------------------------------------
// ToolpathRibbonGLRender.h
//
// Real I3DViewportGLRender implementation — renders a domain::v1::Toolpath
// as flat colored ribbon quads via ToolpathRibbonGLMesh. Registers as
// "toolpath_ribbon" (distinct id from TestGridGLRender's "toolpath"),
// so both are independently selectable in the switcher.
//
// Loads its toolpath data from the msgpack file saved by
// ToolpathEnginePlugin (or a real pipeline run later) — subscribes to
// "toolpath.updated" the same way any real consumer eventually would,
// not a one-off hack.
// -----------------------------------------------------------------------

#include "ports/I3DViewportGLRender.h"
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLMesh.h"

#include <memory>
#include <string>

class ToolpathRibbonGLRender final : public I3DViewportGLRender
{
public:
     ToolpathRibbonGLRender();
    ~ToolpathRibbonGLRender() override;

    int layerCount() const override { return m_mesh.layerCount(); }
    int visibleLayer() const override { return m_visibleLayer; }
    void setVisibleLayer(int layer) override;
    //{ m_visibleLayer = glm::clamp(layer, 0, m_mesh.layerCount() - 1); }

    const char* name() const override { return "Toolpath (ribbon)"; }

    void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) override;
    GLuint getTexture() const override;
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera) override;

    // Loads and rebuilds the mesh from a saved toolpath file — called
    // once at startup for now (test-proof scope), and is the same entry
    // point a future "toolpath.updated" event handler would call.
    void loadToolpath(const std::string& path);

    void setToolpath(const domain::v1::Toolpath& toolpath);


private:
    void ensureGl();
    void ensureFramebuffer(uint32_t width, uint32_t height);

    bool m_glInitialized = false;
    uint32_t m_fboWidth = 0;
    uint32_t m_fboHeight = 0;

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;

    GLuint m_shader = 0;

    ToolpathRibbonGLMesh m_mesh;
    int m_visibleLayer = -1;   // -1 = show all layers
};
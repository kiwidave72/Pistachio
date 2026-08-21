#pragma once

// -----------------------------------------------------------------------
// ToolpathRibbonGLRender.h
//
// Real I3DViewportGLRender implementation - renders a domain::v1::Toolpath
// as flat colored ribbon quads via ToolpathRibbonGLMesh. Registers as
// "toolpath_ribbon" (distinct id from TestGridGLRender's "toolpath"),
// so both are independently selectable in the switcher.
//
// Loads its toolpath data from the msgpack file saved by
// ToolpathEnginePlugin (or a real pipeline run later) - subscribes to
// "toolpath.updated" the same way any real consumer eventually would,
// not a one-off hack.
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"
#include "ports/I3DViewportGLRender.h"
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLMesh.h"

#include <memory>
#include <string>
#include <vector>

#include <chrono> 

class ToolpathRibbonGLRender final : public I3DViewportGLRender
{
public:
    ToolpathRibbonGLRender();
    ~ToolpathRibbonGLRender() override;

    void setVisibleLayerRange(int startLayer, int endLayer);
    int visibleLayerStart() const { return m_layerRangeStart; }
    int visibleLayerEnd() const { return m_layerRangeEnd; }

    int layerCount() const override { return m_mesh.layerCount(); }

    int visibleLayer() const override { return m_visibleLayer; }
    void setVisibleLayer(int layer) override;

    const char* name() const override { return "Toolpath (ribbon)"; }

    void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) override;
    void renderUI(ImVec2 topLeft, ImVec2 size) override;
    GLuint getTexture() const override;
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera) override;

    // Loads and rebuilds the mesh from a saved toolpath file - called
    // once at startup for now (test-proof scope), and is the same entry
    // point a future "toolpath.updated" event handler would call.
    void loadToolpath(const std::string& path);

    void setToolpath(const domain::v1::Toolpath& toolpath);


private:
    void ensureGl();
    void ensureFramebuffer(uint32_t width, uint32_t height);

    // Chunk occlusion culling: reads back last frame's query results
    // (never stalls waiting on this frame's), updates m_chunkVisible,
    // then issues this frame's queries for chunks in the current layer
    // range. See ToolpathRibbonGLMesh::kChunkSize for the grouping.
    void updateChunkVisibility(const glm::mat4& viewProj);
    void resetOcclusionState();

    GLuint m_gridVao = 0, m_gridVbo = 0;
    int m_gridVertexCount = 0;

    float m_fpsAccumTime = 0.0f;
    int   m_fpsFrameCount = 0;
    float m_fpsDisplay = 0.0f;
    std::chrono::steady_clock::time_point m_lastFrameTime = std::chrono::steady_clock::now();

    bool m_glInitialized = false;
    uint32_t m_fboWidth = 0;
    uint32_t m_fboHeight = 0;

    GLuint m_fbo = 0;
    GLuint m_colorTexture = 0;
    GLuint m_depthRenderbuffer = 0;

    GLuint m_shader = 0;

    CameraState m_cameraState;

    ToolpathRibbonGLMesh m_mesh;
    int m_visibleLayer = -1;   // -1 = show all layers

    int m_layerRangeStart = 0;
    int m_layerRangeEnd = -1;

    // One GL_ANY_SAMPLES_PASSED query per mesh chunk. Sized/regenerated
    // whenever setToolpath() rebuilds the mesh (chunk count changes).
    // m_chunkVisible defaults to true so nothing is wrongly culled
    // before the first query result comes back - a chunk only stops
    // drawing once we positively know it's hidden, never speculatively.
    std::vector<GLuint> m_occlusionQueries;
    std::vector<bool> m_chunkQueryPending;
    std::vector<bool> m_chunkVisible;

    domain::v1::ToolpathSegment m_firstLayerFirstSegment;
    domain::v1::ToolpathSegment m_lastLayerLastSegment;
    glm::mat4 m_lastModelMatrix{ 1.0f };
};
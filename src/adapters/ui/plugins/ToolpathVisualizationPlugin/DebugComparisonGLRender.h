#pragma once

// -----------------------------------------------------------------------
// DebugComparisonGLRender.h
//
// Renders the raw STL mesh (recentered at its own bounds center, colored
// by a Z-gradient shader) alongside the toolpath ribbon (independently
// recentered at ITS OWN bounds center) — both with ZERO real-world
// transforms (no instance transform, no bed offset, no axisFix). This
// isolates "does the toolpath data match the source geometry" from "is
// the display transform correct" — the confound that made the earlier
// investigation hard.
//
// Also draws a wireframe bounding box for each, for scale/extent
// comparison.
//
// Camera defaults to distance=200, yaw=0, pitch=0 via
// defaultDistance()/defaultYaw()/defaultPitch() overrides.
// -----------------------------------------------------------------------

#include "ports/I3DViewportGLRender.h"
#include "domain/ModelCache.h"
#include "domain/Toolpath.h"
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLMesh.h"

#include <string>

class DebugComparisonGLRender final : public I3DViewportGLRender
{
public:
    DebugComparisonGLRender();
    ~DebugComparisonGLRender() override;

    const char* name() const override { return "Debug Comparison"; }

    void render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx) override;
    GLuint getTexture() const override;
    RaycastHit raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera) override;

    glm::vec3 defaultTarget() const override { return glm::vec3(0.0f); }
    float defaultDistance() const override { return 200.0f; }
    float defaultYaw() const override { return 0.0f; }
    float defaultPitch() const override { return 0.0f; }


    bool m_showModel = true;
    bool m_showRibbon = true;
    bool m_useSolidShader = false;
    GLuint m_modelSolidShader = 0;

    void setShowModel(bool show) { m_showModel = show; }
    void setShowRibbon(bool show) { m_showRibbon = show; }
    void setUseSolidShader(bool solid) { m_useSolidShader = solid; }

    // Pulls the raw mesh from ModelCache, recenters at its own bounds
    // center, uploads as a Z-gradient-colored mesh.
    void loadModel(domain::v1::ModelCache& cache, const std::string& modelHash);

    // Pulls the current toolpath from ToolpathStore, recenters at its
    // own bounds center independently.
    void loadToolpath(const domain::v1::Toolpath& toolpath);

private:
    void ensureGl();
    void ensureFramebuffer(uint32_t width, uint32_t height);
    void buildBoundsWireframe(const glm::vec3& min, const glm::vec3& max, std::vector<float>& outVerts);

    bool m_glInitialized = false;
    uint32_t m_fboWidth = 0, m_fboHeight = 0;
    GLuint m_fbo = 0, m_colorTexture = 0, m_depthRenderbuffer = 0;

    // STL mesh — Z-gradient shader
    GLuint m_modelShader = 0;
    GLuint m_modelVao = 0, m_modelVbo = 0, m_modelEbo = 0;
    uint32_t m_modelIndexCount = 0;
    float m_modelMinZ = 0.0f, m_modelMaxZ = 1.0f;   // for the gradient shader's normalization
    bool m_hasModel = false;

    // Toolpath ribbon — reuses the existing mesh builder
    ToolpathRibbonGLMesh m_ribbonMesh;
    bool m_hasRibbon = false;

    // Bounding-box wireframes, one per source, drawn with the line shader
    GLuint m_lineShader = 0;
    GLuint m_modelBoundsVao = 0, m_modelBoundsVbo = 0;
    GLuint m_ribbonBoundsVao = 0, m_ribbonBoundsVbo = 0;
};
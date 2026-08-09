#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLMesh.h"

#include <glm/glm.hpp>
#include <cstdio>

namespace {

    struct RibbonVertex
    {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec3 color;
    };

    glm::vec3 colorForMoveType(domain::v1::ToolpathMoveType type)
    {
        switch (type)
        {
        case domain::v1::ToolpathMoveType::OuterWall: return { 0.0f, 0.8f, 0.0f };
        case domain::v1::ToolpathMoveType::InnerWall: return { 0.0f, 0.55f, 0.9f };
        case domain::v1::ToolpathMoveType::Infill:    return { 0.9f, 0.55f, 0.0f };
        case domain::v1::ToolpathMoveType::Skin:      return { 0.9f, 0.0f, 0.8f };
        default:                                        return { 0.6f, 0.6f, 0.6f };
        }
    }

    // Appends one flat quad for a single extruding segment — width
    // perpendicular to the segment direction in the XY plane, lying at
    // the segment's own Z (no vertical thickness, see header comment).
    void appendSegmentQuad(std::vector<RibbonVertex>& verts, std::vector<uint32_t>& idx,
        const domain::v1::ToolpathSegment& seg)
    {
        glm::vec3 a = seg.start.position;
        glm::vec3 b = seg.end.position;
        glm::vec3 dir = b - a;
        float len = glm::length(dir);
        if (len < 1e-6f) return;
        dir /= len;

        glm::vec3 perp(-dir.y, dir.x, 0.0f);   // perpendicular in XY plane
        float halfWidth = seg.extrusionWidth * 0.5f;
        if (halfWidth < 1e-6f) halfWidth = 0.05f;   // fallback so zero-width segments are still visible

        glm::vec3 offset = perp * halfWidth;
        glm::vec3 color = colorForMoveType(seg.moveType);
        glm::vec3 normal(0.0f, 0.0f, 1.0f);

        uint32_t base = (uint32_t)verts.size();
        verts.push_back({ a - offset, normal, color });
        verts.push_back({ a + offset, normal, color });
        verts.push_back({ b + offset, normal, color });
        verts.push_back({ b - offset, normal, color });

        idx.insert(idx.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }

} // anonymous namespace

ToolpathRibbonGLMesh::ToolpathRibbonGLMesh() = default;

ToolpathRibbonGLMesh::~ToolpathRibbonGLMesh()
{
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
}

void ToolpathRibbonGLMesh::ensureGl()
{
    if (m_glInitialized) return;

#ifdef _WIN32
    auto loader = [](const char* name) -> void* {
        void* p = (void*)wglGetProcAddress(name);
        if (!p) {
            static HMODULE gl32 = LoadLibraryA("opengl32.dll");
            if (gl32) p = (void*)GetProcAddress(gl32, name);
        }
        return p;
        };
    if (!gladLoadGLLoader((GLADloadproc)loader)) {
        printf("[ToolpathRibbonGLMesh] gladLoadGLLoader failed\n");
        return;
    }
#endif

    if (!glad_glGenVertexArrays) {
        printf("[ToolpathRibbonGLMesh] GL not available after gladLoadGLLoader\n");
        return;
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);
    m_glInitialized = true;
}

void ToolpathRibbonGLMesh::build(const domain::v1::Toolpath& toolpath)
{
    ensureGl();
    if (!m_glInitialized) return;

    std::vector<RibbonVertex> verts;
    std::vector<uint32_t> idx;
    m_layerCumulativeCounts.clear();
    m_layerCumulativeCounts.reserve(toolpath.layers.size());

    for (auto& layer : toolpath.layers)
    {
        for (auto& seg : layer.segments)
        {
            if (!seg.isExtruding()) continue;   // travel moves skipped — see header comment
            appendSegmentQuad(verts, idx, seg);
        }
        m_layerCumulativeCounts.push_back((uint32_t)idx.size());
    }

    m_totalIndexCount = (uint32_t)idx.size();

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(RibbonVertex), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RibbonVertex), (void*)offsetof(RibbonVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RibbonVertex), (void*)offsetof(RibbonVertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(RibbonVertex), (void*)offsetof(RibbonVertex, color));

    glBindVertexArray(0);

    printf("[ToolpathRibbonGLMesh] built: %zu vertices, %zu indices, %d layers\n",
        verts.size(), idx.size(), (int)m_layerCumulativeCounts.size());
}

uint32_t ToolpathRibbonGLMesh::indexCountForLayer(int layer) const
{
    if (layer < 0 || m_layerCumulativeCounts.empty()) return 0;
    if (layer >= (int)m_layerCumulativeCounts.size()) return m_totalIndexCount;
    return m_layerCumulativeCounts[layer];
}
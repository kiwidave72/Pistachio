#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLMesh.h"

#include <glm/glm.hpp>
#include <cstdio>
#include <limits>

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

    // Appends one flat quad for a single extruding segment - width
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

    // Grows a running AABB to include a segment's two endpoints, padded
    // by the segment's half-width so the proxy box doesn't clip the
    // ribbon geometry it's supposed to stand in for.
    void growAABB(ToolpathChunkAABB& box, const domain::v1::ToolpathSegment& seg)
    {
        float pad = seg.extrusionWidth * 0.5f;
        if (pad < 1e-6f) pad = 0.05f;
        glm::vec3 padVec(pad, pad, pad);

        box.min = glm::min(box.min, seg.start.position - padVec);
        box.min = glm::min(box.min, seg.end.position - padVec);
        box.max = glm::max(box.max, seg.start.position + padVec);
        box.max = glm::max(box.max, seg.end.position + padVec);
    }

} // anonymous namespace

ToolpathRibbonGLMesh::ToolpathRibbonGLMesh() = default;

ToolpathRibbonGLMesh::~ToolpathRibbonGLMesh()
{
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_proxyCubeVao) glDeleteVertexArrays(1, &m_proxyCubeVao);
    if (m_proxyCubeVbo) glDeleteBuffers(1, &m_proxyCubeVbo);
    if (m_proxyCubeEbo) glDeleteBuffers(1, &m_proxyCubeEbo);
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

    buildProxyCube();
}

// Unit cube, 8 verts / 36 indices, position-only (location 0) so it can
// be drawn with the same ribbon shader (uMVP uniform) used everywhere
// else - vColor just comes out constant, which is fine since occlusion
// queries never touch the color buffer (color mask is off when drawn).
void ToolpathRibbonGLMesh::buildProxyCube()
{
    static const glm::vec3 kCubeVerts[8] = {
        {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0},
        {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1},
    };
    static const uint32_t kCubeIdx[36] = {
        0,1,2, 0,2,3,   // -Z
        4,6,5, 4,7,6,   // +Z
        0,4,5, 0,5,1,   // -Y
        3,2,6, 3,6,7,   // +Y
        0,3,7, 0,7,4,   // -X
        1,5,6, 1,6,2,   // +X
    };

    glGenVertexArrays(1, &m_proxyCubeVao);
    glGenBuffers(1, &m_proxyCubeVbo);
    glGenBuffers(1, &m_proxyCubeEbo);

    glBindVertexArray(m_proxyCubeVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_proxyCubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVerts), kCubeVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_proxyCubeEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kCubeIdx), kCubeIdx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);
}

void ToolpathRibbonGLMesh::indexRangeForLayers(int startLayer, int endLayer, uint32_t& outOffset, uint32_t& outCount) const
{
    startLayer = glm::clamp(startLayer, 0, layerCount() - 1);
    endLayer = glm::clamp(endLayer, startLayer, layerCount() - 1);

    outOffset = (startLayer > 0) ? m_layerCumulativeCounts[startLayer - 1] : 0;
    uint32_t endOffset = m_layerCumulativeCounts[endLayer];
    outCount = endOffset - outOffset;
}

void ToolpathRibbonGLMesh::indexRangeForChunk(int chunk, uint32_t& outOffset, uint32_t& outCount) const
{
    if (chunk < 0 || chunk >= (int)m_chunkIndexOffsets.size())
    {
        outOffset = 0;
        outCount = 0;
        return;
    }
    outOffset = m_chunkIndexOffsets[chunk];
    outCount = m_chunkIndexCounts[chunk];
}

void ToolpathRibbonGLMesh::build(const domain::v1::Toolpath& toolpath)
{
    ensureGl();
    if (!m_glInitialized) return;

    std::vector<RibbonVertex> verts;
    std::vector<uint32_t> idx;
    m_layerCumulativeCounts.clear();
    m_layerCumulativeCounts.reserve(toolpath.layers.size());

    m_chunkAABBs.clear();
    m_chunkIndexOffsets.clear();
    m_chunkIndexCounts.clear();

    const float kInf = std::numeric_limits<float>::max();
    ToolpathChunkAABB chunkBox{ glm::vec3(kInf), glm::vec3(-kInf) };
    uint32_t chunkStartOffset = 0;
    bool chunkHasGeometry = false;

    for (size_t layerIdx = 0; layerIdx < toolpath.layers.size(); ++layerIdx)
    {
        auto& layer = toolpath.layers[layerIdx];

        for (auto& seg : layer.segments)
        {
            if (!seg.isExtruding()) continue;   // travel moves skipped - see header comment
            appendSegmentQuad(verts, idx, seg);
            growAABB(chunkBox, seg);
            chunkHasGeometry = true;
        }
        m_layerCumulativeCounts.push_back((uint32_t)idx.size());

        // Close out a chunk every kChunkSize layers, or on the final
        // layer if it doesn't land exactly on a chunk boundary.
        bool lastLayer = (layerIdx + 1 == toolpath.layers.size());
        if ((layerIdx + 1) % ToolpathRibbonGLMesh::kChunkSize == 0 || lastLayer)
        {
            uint32_t chunkEndOffset = (uint32_t)idx.size();

            // Empty chunk (e.g. no extruding segments at all in this
            // span) still needs an entry so chunk indices line up with
            // occlusion-query arrays sized by chunkCount() - give it a
            // degenerate zero-volume box so it's cheap and harmless to
            // query, and a zero-length draw range.
            if (!chunkHasGeometry)
                chunkBox = ToolpathChunkAABB{ glm::vec3(0.0f), glm::vec3(0.0f) };

            m_chunkAABBs.push_back(chunkBox);
            m_chunkIndexOffsets.push_back(chunkStartOffset);
            m_chunkIndexCounts.push_back(chunkEndOffset - chunkStartOffset);

            chunkStartOffset = chunkEndOffset;
            chunkBox = ToolpathChunkAABB{ glm::vec3(kInf), glm::vec3(-kInf) };
            chunkHasGeometry = false;
        }
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

    printf("[ToolpathRibbonGLMesh] built: %zu vertices, %zu indices, %d layers, %d chunks\n",
        verts.size(), idx.size(), (int)m_layerCumulativeCounts.size(), (int)m_chunkAABBs.size());
}

uint32_t ToolpathRibbonGLMesh::indexCountForLayer(int layer) const
{
    if (layer < 0 || m_layerCumulativeCounts.empty()) return 0;
    if (layer >= (int)m_layerCumulativeCounts.size()) return m_totalIndexCount;
    return m_layerCumulativeCounts[layer];
}
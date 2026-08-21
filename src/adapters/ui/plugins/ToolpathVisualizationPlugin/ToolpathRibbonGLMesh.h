#pragma once

// -----------------------------------------------------------------------
// ToolpathRibbonGLMesh.h
//
// Built once per loaded/generated Toolpath, cached - not rebuilt per
// frame or per layer-scrub. "Show up to layer N" is a draw-count change
// only (indexCountForLayer returns a CUMULATIVE count through layer N),
// never a geometry rebuild.
//
// Flat horizontal quads per extruding segment for this first version -
// width = extrusionWidth, no vertical thickness. A full extruded-box
// upgrade (visible side walls) is a natural later step, not built now.
// Travel moves (isExtruding() == false) are skipped entirely - a
// phantom-filament ribbon would look wrong.
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// Axis-aligned bounds for a chunk of layers, used as the occlusion-query
// proxy volume. min/max are in the mesh's own local (pre-model-matrix)
// space, same space as the ribbon vertices themselves.
struct ToolpathChunkAABB
{
    glm::vec3 min{ 0.0f };
    glm::vec3 max{ 0.0f };
};

class ToolpathRibbonGLMesh
{
public:
    ToolpathRibbonGLMesh();
    ~ToolpathRibbonGLMesh();

    void build(const domain::v1::Toolpath& toolpath);

    GLuint vao() const { return m_vao; }

    void indexRangeForLayers(int startLayer, int endLayer, uint32_t& outOffset, uint32_t& outCount) const;

    // Cumulative index count through layer N (inclusive) - pass directly
    // as the count argument to glDrawElements, always starting from
    // index 0. layerCount() gives the valid range for a scrubber UI.
    uint32_t indexCountForLayer(int layer) const;
    int layerCount() const { return (int)m_layerCumulativeCounts.size(); }

    // --- Chunk-based occlusion culling support -----------------------
    // Layers are grouped into fixed-size chunks (see kChunkSize) purely
    // for occlusion-query granularity - NOT a rebuild boundary, NOT
    // related to indexRangeForLayers/indexCountForLayer which stay
    // per-layer for the existing layer-scrubber UI. A chunk's index
    // range is simply the union of its member layers' ranges, so the
    // occlusion pass can draw "chunk 3" as a single glDrawElements call
    // when it turns out to be visible.
    static constexpr int kChunkSize = 16;

    int chunkCount() const { return (int)m_chunkAABBs.size(); }
    const ToolpathChunkAABB& chunkBounds(int chunk) const { return m_chunkAABBs[chunk]; }
    void indexRangeForChunk(int chunk, uint32_t& outOffset, uint32_t& outCount) const;

    // Unit cube (0..1 on each axis) used as the occlusion-query proxy
    // for a chunk - scaled/translated into a chunk's AABB via a model
    // matrix built from chunkBounds(), never re-uploaded per chunk.
    GLuint proxyCubeVao() const { return m_proxyCubeVao; }

private:
    void ensureGl();
    void buildProxyCube();

    bool m_glInitialized = false;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    uint32_t m_totalIndexCount = 0;

    std::vector<uint32_t> m_layerCumulativeCounts;

    std::vector<ToolpathChunkAABB> m_chunkAABBs;
    std::vector<uint32_t> m_chunkIndexOffsets;
    std::vector<uint32_t> m_chunkIndexCounts;

    GLuint m_proxyCubeVao = 0;
    GLuint m_proxyCubeVbo = 0;
    GLuint m_proxyCubeEbo = 0;
};
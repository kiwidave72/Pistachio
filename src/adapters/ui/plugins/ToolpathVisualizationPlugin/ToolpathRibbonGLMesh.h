#pragma once

// -----------------------------------------------------------------------
// ToolpathRibbonGLMesh.h
//
// Built once per loaded/generated Toolpath, cached — not rebuilt per
// frame or per layer-scrub. "Show up to layer N" is a draw-count change
// only (indexCountForLayer returns a CUMULATIVE count through layer N),
// never a geometry rebuild.
//
// Flat horizontal quads per extruding segment for this first version —
// width = extrusionWidth, no vertical thickness. A full extruded-box
// upgrade (visible side walls) is a natural later step, not built now.
// Travel moves (isExtruding() == false) are skipped entirely — a
// phantom-filament ribbon would look wrong.
//
// Per-move-type color baked in as a per-vertex attribute (cheap, no
// shader-strategy system needed for a first pass) — matches the design
// doc's already-planned "color by move type" shading mode.
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"

#include <glad/glad.h>
#include <vector>
#include <cstdint>

class ToolpathRibbonGLMesh
{
public:
    ToolpathRibbonGLMesh();
    ~ToolpathRibbonGLMesh();

    void build(const domain::v1::Toolpath& toolpath);

    GLuint vao() const { return m_vao; }

    // Cumulative index count through layer N (inclusive) — pass directly
    // as the count argument to glDrawElements, always starting from
    // index 0. layerCount() gives the valid range for a scrubber UI.
    uint32_t indexCountForLayer(int layer) const;
    int layerCount() const { return (int)m_layerCumulativeCounts.size(); }

private:
    void ensureGl();

    bool m_glInitialized = false;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    uint32_t m_totalIndexCount = 0;

    std::vector<uint32_t> m_layerCumulativeCounts;
};
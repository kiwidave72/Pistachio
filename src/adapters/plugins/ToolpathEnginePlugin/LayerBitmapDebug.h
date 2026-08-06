#pragma once

// -----------------------------------------------------------------------
// LayerBitmapDebug.h
//
// Rasterizes one layer's chains to a PNG for visual inspection — purely
// 2D pixel math, no GL/viewport dependency at all. Independent of the
// (still-blocked) viewport refactor. Debug/dev tool only, not part of
// the real pipeline output.
//
// Green = naturally closed chain, yellow = repaired, red = still open.
// -----------------------------------------------------------------------

#include "domain/AdvancedSliceResult.h"
#include "domain/SegmentChain.h"

#include <string>

namespace kinetica {

    class LayerBitmapDebug
    {
    public:
        static void dumpLayer(
            const domain::v1::ExtractedLayer& layer,
            const std::string& outputPath,
            int imageSize = 1024);
    };

} // namespace kinetica
#pragma once

#include "domain/AdvancedSliceResult.h"
#include "domain/SegmentChain.h"
#include "domain/AdvancedTopology.h"
#include "adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/TopologyPhase.h"

#include <string>
#include <vector>

namespace kinetica {

    class LayerBitmapDebug
    {
    public:
        static void dumpChainLayer(
            const domain::v1::ExtractedLayer& layer,
            const std::string& outputDir,
            const std::string& modelInstanceId,
            int imageSize = 1024);

        static void dumpTopologyLayer(
            const domain::v1::TopologyLayer& layer,
            const std::string& outputDir,
            const std::string& modelInstanceId,
            int imageSize = 1024);

        static void dumpAllChainLayers(
            const std::vector<ExtractedGeometry>& allInstances,
            const std::string& outputDir);

        static void dumpAllTopologyLayers(
            const std::vector<TopologizedGeometry>& allInstances,
            const std::string& outputDir);
 
        static void writeRunReport(
            const std::vector<ExtractedGeometry>& extracted,
            const std::vector<TopologizedGeometry>& topologized,
            const std::string& outputDir);

        static std::string buildPath(
            const std::string& outputDir,
            const std::string& modelInstanceId,
            const std::string& prefix,
            int layerIndex);

        static std::string buildFilename(
            const std::string& modelInstanceId,
            const std::string& prefix,
            int layerIndex,
            const std::string& extension);   // ".png" or ".json"
    };

} // namespace kinetica
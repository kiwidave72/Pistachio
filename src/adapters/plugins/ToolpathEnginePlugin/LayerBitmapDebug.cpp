#include "adapters/plugins/ToolpathEnginePlugin/LayerBitmapDebug.h"
#include "adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/TopologyPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/WallGenerationPhase.h"



#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <vector>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <nlohmann/json.hpp>

namespace kinetica {

    namespace {

        struct RGB { uint8_t r, g, b; };

        void setPixel(std::vector<uint8_t>& img, int size, int x, int y, RGB c)
        {
            if (x < 0 || y < 0 || x >= size || y >= size) return;
            int idx = (y * size + x) * 3;
            img[idx + 0] = c.r;
            img[idx + 1] = c.g;
            img[idx + 2] = c.b;
        }

        void drawLine(std::vector<uint8_t>& img, int size,
            int x0, int y0, int x1, int y1, RGB c, int thickness = 2)
        {
            int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy;

            while (true)
            {
                for (int ox = -thickness; ox <= thickness; ++ox)
                    for (int oy = -thickness; oy <= thickness; ++oy)
                        setPixel(img, size, x0 + ox, y0 + oy, c);

                if (x0 == x1 && y0 == y1) break;
                int e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        }

        // Shared: compute bounds + scale for a set of point-lists, return
        // enough info to build a toPixel() closure. Works identically for
        // chains and contours, since both reduce to vector<glm::vec3>.
        // pointLists is a collection of POINTERS to point vectors — must
        // dereference each element before iterating its points.
        template<typename PointListRange>
        bool computeTransform(const PointListRange& pointLists, int imageSize,
            glm::vec2& outMinP, float& outScale, float& outMargin)
        {
            glm::vec2 minP{ 1e30f }, maxP{ -1e30f };
            bool any = false;

            for (auto* points : pointLists)
                for (auto& p : *points)
                {
                    any = true;
                    minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y);
                    maxP.x = std::max(maxP.x, p.x); maxP.y = std::max(maxP.y, p.y);
                }

            if (!any) return false;

            glm::vec2 extent = maxP - minP;
            float maxExtent = std::max(extent.x, extent.y);
            if (maxExtent < 1e-6f) return false;

            outMargin = imageSize * 0.05f;
            outScale = (imageSize - 2 * outMargin) / maxExtent;
            outMinP = minP;
            return true;
        }

        bool writePng(const std::vector<uint8_t>& img, int imageSize, const std::string& path)
        {
            int ok = stbi_write_png(path.c_str(), imageSize, imageSize, 3, img.data(), imageSize * 3);
            if (ok)
                printf("[LayerBitmapDebug] wrote %s\n", path.c_str());
            else
                printf("[LayerBitmapDebug] FAILED to write %s\n", path.c_str());
            return ok != 0;
        }

        nlohmann::json diagnosticToJson(const domain::v1::DiagnosticMessage& d)
        {
            nlohmann::json j;
            j["severity"] = (d.severity == domain::v1::DiagnosticSeverity::Error) ? "error"
                : (d.severity == domain::v1::DiagnosticSeverity::Warning) ? "warning" : "info";
            j["phase"] = d.phase;
            j["message"] = d.message;
            if (d.hasLocation)
                j["location"] = { {"x", d.location.x}, {"y", d.location.y}, {"z", d.location.z} };
            return j;
        }

        // One layer's full report — same shape written both as a
        // standalone sidecar and as one entry in the aggregate file.
        nlohmann::json buildLayerReport(
            const std::string& modelInstanceId, int layerIndex, float z,
            const std::vector<domain::v1::DiagnosticMessage>& chainDiagnostics,
            const std::vector<domain::v1::DiagnosticMessage>& topologyDiagnostics)
        {
            nlohmann::json j;
            j["modelInstanceId"] = modelInstanceId;
            j["layerIndex"] = layerIndex;
            j["z"] = z;
            j["chainPng"] = modelInstanceId + "_chains" +
                LayerBitmapDebug::buildPath("", "", "", layerIndex).substr(1); // reuse padding logic
            j["topologyPng"] = modelInstanceId + "_topology" +
                LayerBitmapDebug::buildPath("", "", "", layerIndex).substr(1);

            nlohmann::json diag = nlohmann::json::array();
            for (auto& d : chainDiagnostics) diag.push_back(diagnosticToJson(d));
            for (auto& d : topologyDiagnostics) diag.push_back(diagnosticToJson(d));
            j["diagnostics"] = diag;

            return j;
        }
       
        

    } // anonymous namespace

   
    std::string LayerBitmapDebug::buildPath(
        const std::string& outputDir,
        const std::string& modelInstanceId,
        const std::string& prefix,
        int layerIndex)
    {
        return outputDir + "\\" + buildFilename(modelInstanceId, prefix, layerIndex, ".png");
    }

    std::string LayerBitmapDebug::buildFilename(
        const std::string& modelInstanceId,
        const std::string& prefix,
        int layerIndex,
        const std::string& extension)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%03d", layerIndex);
        return modelInstanceId + "_" + prefix + buf + extension;
    }

   

    void LayerBitmapDebug::writeRunReport(
        const std::vector<ExtractedGeometry>& extracted,
        const std::vector<TopologizedGeometry>& topologized,
        const std::string& outputDir)
    {
        nlohmann::json runReport;
        runReport["layers"] = nlohmann::json::array();

        for (size_t instIdx = 0; instIdx < extracted.size(); ++instIdx)
        {
            auto& extInst = extracted[instIdx];
            auto* topoInst = (instIdx < topologized.size()) ? &topologized[instIdx] : nullptr;

            for (size_t layerIdx = 0; layerIdx < extInst.layers.size(); ++layerIdx)
            {
                auto& chainLayer = extInst.layers[layerIdx];
                const std::vector<domain::v1::DiagnosticMessage>* topoDiag = nullptr;
                if (topoInst && layerIdx < topoInst->topology.layers.size())
                    topoDiag = &topoInst->topology.layers[layerIdx].diagnostics;

                auto layerReport = buildLayerReport(
                    extInst.modelInstanceId, chainLayer.layerIndex, chainLayer.z,
                    chainLayer.diagnostics,
                    topoDiag ? *topoDiag : std::vector<domain::v1::DiagnosticMessage>{});

                // Per-layer sidecar — same object, written standalone too
                std::string sidecarPath = buildPath(outputDir, extInst.modelInstanceId, "layer", chainLayer.layerIndex);
                sidecarPath = sidecarPath.substr(0, sidecarPath.size() - 4) + ".json"; // swap .png -> .json
                std::ofstream sidecar(sidecarPath);
                if (sidecar) sidecar << layerReport.dump(2);

                runReport["layers"].push_back(layerReport);
            }
        }

        std::ofstream runFile(outputDir + "\\run.json");
        if (runFile) runFile << runReport.dump(2);

        printf("[LayerBitmapDebug] wrote run report: %s\\run.json\n", outputDir.c_str());
    }


    // LayerBitmapDebug.cpp — add
    void LayerBitmapDebug::dumpToolpathLayer(
        const domain::v1::ToolpathLayer& layer,
        int layerIndex,
        const std::string& outputDir,
        const std::string& modelInstanceId,
        int imageSize)
    {
        if (layer.segments.empty()) return;

        std::vector<std::vector<glm::vec3>> pointLists;   // one 2-point "list" per segment, for bounds computation
        for (auto& seg : layer.segments)
            pointLists.push_back({ seg.start.position, seg.end.position });

        std::vector<const std::vector<glm::vec3>*> pointListPtrs;
        for (auto& pl : pointLists) pointListPtrs.push_back(&pl);

        glm::vec2 minP; float scale, margin;
        if (!computeTransform(pointListPtrs, imageSize, minP, scale, margin)) return;

        auto toPixel = [&](const glm::vec3& p) -> std::pair<int, int>
            {
                int px = static_cast<int>(margin + (p.x - minP.x) * scale);
                int py = static_cast<int>(imageSize - (margin + (p.y - minP.y) * scale));
                return { px, py };
            };

        std::vector<uint8_t> img(imageSize * imageSize * 3, 30);

        auto colorFor = [](domain::v1::ToolpathMoveType type) -> RGB
            {
                switch (type)
                {
                case domain::v1::ToolpathMoveType::OuterWall: return { 0, 200, 0 };
                case domain::v1::ToolpathMoveType::InnerWall: return { 0, 140, 230 };
                case domain::v1::ToolpathMoveType::Infill:    return { 230, 140, 0 };
                case domain::v1::ToolpathMoveType::Skin:      return { 230, 0, 200 };
                default:                                        return { 150, 150, 150 };
                }
            };

        for (auto& seg : layer.segments)
        {
            RGB color = colorFor(seg.moveType);
            auto [x0, y0] = toPixel(seg.start.position);
            auto [x1, y1] = toPixel(seg.end.position);
            drawLine(img, imageSize, x0, y0, x1, y1, color);
        }

        writePng(img, imageSize, buildPath(outputDir, modelInstanceId, "toolpath", layerIndex));
    }

    void LayerBitmapDebug::dumpAllChainLayers(
        const std::vector<ExtractedGeometry>& allInstances,
        const std::string& outputDir)
    {
        for (auto& instance : allInstances)
            for (auto& layer : instance.layers)
                dumpChainLayer(layer, outputDir, instance.modelInstanceId);
    }

    void LayerBitmapDebug::dumpAllTopologyLayers(
        const std::vector<TopologizedGeometry>& allInstances,
        const std::string& outputDir)
    {
        for (auto& instance : allInstances)
            for (auto& layer : instance.topology.layers)
                dumpTopologyLayer(layer, outputDir, instance.topology.modelInstanceId);
    }

    

    void LayerBitmapDebug::dumpChainLayer(
        const domain::v1::ExtractedLayer& layer,
        const std::string& outputDir,
        const std::string& modelInstanceId,
        int imageSize)
    {
        if (layer.chains.empty()) return;

        std::vector<const std::vector<glm::vec3>*> pointLists;
        for (auto& chain : layer.chains) pointLists.push_back(&chain.points);

        glm::vec2 minP; float scale, margin;
        if (!computeTransform(pointLists, imageSize, minP, scale, margin)) return;

        auto toPixel = [&](const glm::vec3& p) -> std::pair<int, int>
            {
                int px = static_cast<int>(margin + (p.x - minP.x) * scale);
                int py = static_cast<int>(imageSize - (margin + (p.y - minP.y) * scale));
                return { px, py };
            };

        std::vector<uint8_t> img(imageSize * imageSize * 3, 30);

        for (auto& chain : layer.chains)
        {
            RGB color = chain.isClosed
                ? (chain.wasRepaired ? RGB{ 230, 200, 0 } : RGB{ 0, 200, 0 })
                : RGB{ 220, 30, 30 };

            for (size_t i = 0; i + 1 < chain.points.size(); ++i)
            {
                auto [x0, y0] = toPixel(chain.points[i]);
                auto [x1, y1] = toPixel(chain.points[i + 1]);
                drawLine(img, imageSize, x0, y0, x1, y1, color);
            }
        }

        writePng(img, imageSize, buildPath(outputDir, modelInstanceId, "chains", layer.layerIndex));
    }

    void LayerBitmapDebug::dumpTopologyLayer(
        const domain::v1::TopologyLayer& layer,
        const std::string& outputDir,
        const std::string& modelInstanceId,
        int imageSize)
    {
        if (layer.contours.empty()) return;

        std::vector<const std::vector<glm::vec3>*> pointLists;
        for (auto& contour : layer.contours) pointLists.push_back(&contour.points);

        glm::vec2 minP; float scale, margin;
        if (!computeTransform(pointLists, imageSize, minP, scale, margin)) return;

        auto toPixel = [&](const glm::vec3& p) -> std::pair<int, int>
            {
                int px = static_cast<int>(margin + (p.x - minP.x) * scale);
                int py = static_cast<int>(imageSize - (margin + (p.y - minP.y) * scale));
                return { px, py };
            };

        std::vector<uint8_t> img(imageSize * imageSize * 3, 30);

        for (auto& contour : layer.contours)
        {
            // Green = outer boundary, blue = hole. Distinct from the chain
            // view's palette so the two are never visually confused.
            RGB color = contour.isOuter ? RGB{ 0, 200, 0 } : RGB{ 60, 140, 230 };

            for (size_t i = 0; i < contour.points.size(); ++i)
            {
                size_t next = (i + 1) % contour.points.size();   // contours are closed — wrap around
                auto [x0, y0] = toPixel(contour.points[i]);
                auto [x1, y1] = toPixel(contour.points[next]);
                drawLine(img, imageSize, x0, y0, x1, y1, color);
            }
        }

        writePng(img, imageSize, buildPath(outputDir, modelInstanceId, "topology", layer.layerIndex));
    }

} // namespace kinetica
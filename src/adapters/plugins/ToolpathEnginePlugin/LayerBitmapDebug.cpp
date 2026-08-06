#include "adapters/plugins/ToolpathEnginePlugin/LayerBitmapDebug.h"

#include "stb_image_write.h"

#include <vector>
#include <algorithm>
#include <cstdio>
#include <cmath>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

        // Simple thick-line draw (a few pixels wide) so thin segments
        // are actually visible at typical image sizes.
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

    } // anonymous namespace

    void LayerBitmapDebug::dumpLayer(
        const domain::v1::ExtractedLayer& layer,
        const std::string& outputPath,
        int imageSize)
    {
        if (layer.chains.empty()) return;

        // Compute this layer's own X/Y bounds across all chains, so it
        // fills the image regardless of the part's absolute bed position.
        glm::vec2 minP{ 1e30f }, maxP{ -1e30f };
        for (auto& chain : layer.chains)
            for (auto& p : chain.points)
            {
                minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y);
                maxP.x = std::max(maxP.x, p.x); maxP.y = std::max(maxP.y, p.y);
            }

        glm::vec2 extent = maxP - minP;
        float maxExtent = std::max(extent.x, extent.y);
        if (maxExtent < 1e-6f) return;

        float margin = imageSize * 0.05f;
        float scale = (imageSize - 2 * margin) / maxExtent;

        auto toPixel = [&](const glm::vec3& p) -> std::pair<int, int>
            {
                int px = static_cast<int>(margin + (p.x - minP.x) * scale);
                int py = static_cast<int>(imageSize - (margin + (p.y - minP.y) * scale)); // flip Y for image coords
                return { px, py };
            };

        std::vector<uint8_t> img(imageSize * imageSize * 3, 30);   // dark grey background

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

        int ok = stbi_write_png(outputPath.c_str(), imageSize, imageSize, 3, img.data(), imageSize * 3);
        if (ok)
            printf("[LayerBitmapDebug] wrote %s\n", outputPath.c_str());
        else
            printf("[LayerBitmapDebug] FAILED to write %s\n", outputPath.c_str());
    }

} // namespace kinetica
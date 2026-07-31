 
#include "adapters/loaders/StlMeshLoader.h"
#include "domain/Mesh.hpp"
#include "domain/Model.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>  // for std::transform, tolower

namespace adapters {

    struct Point3DHash {
        std::size_t operator()(const domain::Point3D& p) const noexcept {
            std::size_t h = 2166136261u;  // FNV-1a offset basis
            for (float f : p) {
                uint32_t bits;
                std::memcpy(&bits, &f, sizeof(bits));  // reinterpret float bits as uint32
                h ^= bits;
                h *= 16777619u;  // FNV prime
            }
            return h;
        }
    };


    std::string StlMeshLoader::getSupportedExtensions() const {
        return ".stl, .STL";
    }

    bool StlMeshLoader::hasStepExtension(const std::string& filepath) const {
        // Fix: was missing `return`
        if (filepath.size() < 4) return false;
        std::string ext = filepath.substr(filepath.rfind('.'));
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".stl";
    }

    std::shared_ptr<domain::Model> StlMeshLoader::load(
        const std::string& path,
        ports::ProgressCallback progressCallback)
    {
        // ── Open ─────────────────────────────────────────────────────────────────
        std::ifstream f(path, std::ios::binary);
        if (!f)
            throw std::runtime_error("Cannot open file: " + path);

        // ── STL binary header + triangle count ───────────────────────────────────
        f.seekg(80, std::ios::beg);          // skip 80-byte header
        uint32_t numTriangles{};
        f.read(reinterpret_cast<char*>(&numTriangles), sizeof(numTriangles));
        if (!f)
            throw std::runtime_error("Truncated STL header: " + path);

        // ── Model + Geometry ─────────────────────────────────────────────────────
        auto model = std::make_shared<domain::Model>();
        model->setName(path);
        
        auto geometry = std::make_shared<domain::Geometry>();
        // Vertex deduplication: exact Point3D bits → index
        std::unordered_map<domain::Point3D, size_t, Point3DHash> vertexIndex;
        vertexIndex.reserve(static_cast<size_t>(numTriangles) * 2);
         
        // ── Parse triangles ───────────────────────────────────────────────────────
        for (uint32_t i = 0; i < numTriangles; ++i) {
            float    buf[12];   // [0-2] normal, [3-11] three vertices
            uint16_t attr{};

            f.read(reinterpret_cast<char*>(buf), 48);
            f.read(reinterpret_cast<char*>(&attr), 2);
            if (!f)
                throw std::runtime_error(
                    "Truncated STL data at triangle " + std::to_string(i));

            domain::Triangle tri{};
            for (int v = 0; v < 3; ++v) {
                domain::Point3D pt{
                    buf[3 + v * 3],
                    buf[4 + v * 3],
                    buf[5 + v * 3]
                };
                auto [it, inserted] = vertexIndex.emplace(pt, geometry->getVertices().size());
                if (inserted)
                    geometry->addVertex(pt);
               
         

                tri[v] = it->second;
            }
            geometry->addTriangle(tri);
            if (progressCallback && numTriangles > 0)
                progressCallback("Loading STL",
                    static_cast<float>(i + 1) / static_cast<float>(numTriangles) * 100.0f);
        }

        // ── Attach geometry to model ──────────────────────────────────────────────
        model->addGeometry(geometry);

        if (progressCallback)
            progressCallback("Complete!", 100.0f);

        return model;
    }

    bool StlMeshLoader::canLoad(const std::string& filepath) const {
        return hasStepExtension(filepath);  // Fix: delegate to extension check
    }

    
} // namespace adapters


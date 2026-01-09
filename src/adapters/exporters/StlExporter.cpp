#include "adapters/exporters/StlExporter.h"
#include <fstream>
#include <stdexcept>
#include <cmath>

namespace adapters {

bool StlExporter::exportModel(const domain::Model& model, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }
    
    file << "solid " << model.getName() << "\n";
    
    for (const auto& geometry : model.getGeometries()) {
        const auto& vertices = geometry->getVertices();
        
        for (const auto& triangle : geometry->getTriangles()) {
            const auto& v0 = vertices[triangle[0]];
            const auto& v1 = vertices[triangle[1]];
            const auto& v2 = vertices[triangle[2]];
            
            // Calculate normal vector
            float u[3] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2]};
            float v[3] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2]};
            
            float nx = u[1] * v[2] - u[2] * v[1];
            float ny = u[2] * v[0] - u[0] * v[2];
            float nz = u[0] * v[1] - u[1] * v[0];
            
            float length = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (length > 0.0f) {
                nx /= length;
                ny /= length;
                nz /= length;
            }
            
            file << "  facet normal " << nx << " " << ny << " " << nz << "\n";
            file << "    outer loop\n";
            file << "      vertex " << v0[0] << " " << v0[1] << " " << v0[2] << "\n";
            file << "      vertex " << v1[0] << " " << v1[1] << " " << v1[2] << "\n";
            file << "      vertex " << v2[0] << " " << v2[1] << " " << v2[2] << "\n";
            file << "    endloop\n";
            file << "  endfacet\n";
        }
    }
    
    file << "endsolid " << model.getName() << "\n";
    file.close();
    return true;
}

std::string StlExporter::getSupportedExtension() const {
    return "stl";
}

} // namespace adapters
#include "adapters/exporters/ObjExporter.h"
#include <fstream>
#include <stdexcept>

namespace adapters {

bool ObjExporter::exportModel(const domain::Model& model, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }
    
    file << "# Exported by Pistachio - CAD Converter\n";
    file << "# Model: " << model.getName() << "\n\n";
    
    size_t vertexOffset = 1;
    for (const auto& geometry : model.getGeometries()) {
        file << "o Geometry\n";
        
        // Write vertices
        for (const auto& vertex : geometry->getVertices()) {
            file << "v " << vertex[0] << " " << vertex[1] << " " << vertex[2] << "\n";
        }
        
        // Write faces
        for (const auto& triangle : geometry->getTriangles()) {
            file << "f " 
                 << (triangle[0] + vertexOffset) << " "
                 << (triangle[1] + vertexOffset) << " "
                 << (triangle[2] + vertexOffset) << "\n";
        }
        
        vertexOffset += geometry->getVertices().size();
    }
    
    file.close();
    return true;
}

std::string ObjExporter::getSupportedExtension() const {
    return "obj";
}

} // namespace adapters
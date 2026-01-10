#include "JsonSketchDocumentAdapter.h"

#include <fstream>
#include <stdexcept>

#include "SketchDocumentJson.h"
#include "SketchDocumentMapper.h"

namespace adapters::persistence {

    static bool endsWith(const std::string& s, const std::string& suffix) {
        if (suffix.size() > s.size()) return false;
        return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
    }

    std::shared_ptr<domain::sketch::Document> JsonSketchDocumentAdapter::loadDocument(const std::string& filepath) {
        std::ifstream in(filepath, std::ios::in | std::ios::binary);
        if (!in) {
            throw std::runtime_error("Failed to open sketch file for reading: " + filepath);
        }

        nlohmann::json j;

        in >> j;

        dto::FileDto file = j.get<dto::FileDto>();
        return SketchDocumentMapper::fromDto(file);
    }

    void JsonSketchDocumentAdapter::saveDocument(const domain::sketch::Document& doc, const std::string& filepath) {
        dto::FileDto file = SketchDocumentMapper::toDto(doc);
        nlohmann::json j = file;

        std::ofstream out(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out) {
            throw std::runtime_error("Failed to open sketch file for writing: " + filepath);
        }

        // Pretty print for now. If you want smaller files, change to dump() with indent = -1.
        out << j.dump(2);
    }

    bool JsonSketchDocumentAdapter::canHandle(const std::string& filepath) const {
        return endsWith(filepath, ".sketch.json") || endsWith(filepath, ".pistachio.json");
    }

    std::string JsonSketchDocumentAdapter::supportedExtensions() const {
        return "*.sketch.json;*.pistachio.json";
    }

} // namespace adapters::persistence

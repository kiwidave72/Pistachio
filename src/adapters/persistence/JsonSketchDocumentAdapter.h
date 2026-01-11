#pragma once

#include <memory>
#include <string>

#include "ports/ISketchDocumentPersistencePort.h"

namespace adapters::persistence {

    class JsonSketchDocumentAdapter final : public ports::ISketchDocumentPersistencePort {
    public:
        std::shared_ptr<domain::sketch::Document> loadDocument(const std::string& filepath) override;
        void saveDocument(const domain::sketch::Document& doc, const std::string& filepath) override;

        bool canHandle(const std::string& filepath) const override;
        std::string supportedExtensions() const override;
    };

} // namespace adapters::persistence

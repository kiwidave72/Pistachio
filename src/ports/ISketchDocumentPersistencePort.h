#pragma once

#include <memory>
#include <string>

#include "domain/SketchModel.h"

namespace ports {

    class ISketchDocumentPersistencePort {
    public:
        virtual ~ISketchDocumentPersistencePort() = default;

        virtual std::shared_ptr<domain::sketch::Document> loadDocument(const std::string& filepath) = 0;
        virtual void saveDocument(const domain::sketch::Document& doc, const std::string& filepath) = 0;

        virtual bool canHandle(const std::string& filepath) const = 0;
        virtual std::string supportedExtensions() const = 0;
    };

} // namespace ports

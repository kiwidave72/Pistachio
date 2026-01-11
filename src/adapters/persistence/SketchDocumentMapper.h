#pragma once

#include <memory>

#include "domain/SketchModel.h"
#include "SketchDocumentDto.h"

namespace adapters::persistence {

    // Pure translation layer between persistence DTOs and the runtime domain model.
    // The app/solver can evolve independently of the on-disk schema.
    class SketchDocumentMapper {
    public:
        static dto::FileDto toDto(const domain::sketch::Document& doc);
        static std::shared_ptr<domain::sketch::Document> fromDto(const dto::FileDto& file);
    };

} // namespace adapters::persistence

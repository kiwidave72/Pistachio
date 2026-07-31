#pragma once
#include <memory>
#include "domain/dataContext.h"
    class ModelInstanceFactory
    {
    public:
        ModelInstanceFactory() = default;
        ~ModelInstanceFactory() = default;

        /// Creates a new ModelInstance from an ImportedAsset.
        /// If the model does not exist in the cache it returns nullptr.
        std::unique_ptr<ModelInstance> create(const  domain::v1::ImportedAsset& asset,  Mesh* mesh) const;
        std::vector<std::unique_ptr<ModelInstance>> ModelInstanceFactory::createFromFileMeta(const domain::v1::ImportedAsset& asset, Mesh* mesh) const;

    };
 
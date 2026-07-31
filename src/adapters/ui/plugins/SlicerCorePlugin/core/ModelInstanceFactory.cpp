#include "ModelInstanceFactory.h"
#include "domain/dataContext.h"
#include "core/GuidUtils.h"
#include <glm/vec3.hpp>

std::vector<std::unique_ptr<ModelInstance>> ModelInstanceFactory::createFromFileMeta(const domain::v1::ImportedAsset& asset, Mesh* mesh) const
{
    auto meta = ParsedFileMeta::parse(asset.fileName);
    auto collection =  std::vector<std::unique_ptr<ModelInstance>>();
        
    // get the quantity from the filename meta data.
    for (int i = 0;i < meta.quantity;i++) {
        auto instance = std::make_unique<ModelInstance>();
        instance->id = utils::generateGuid();
        instance->name = asset.name + "["+ std::to_string(i) +"]";
        instance->modelHash = asset.Id;
        instance->transform.position = glm::vec3(0.0f);
        instance->transform.rotation = glm::vec3(0.0f);
        instance->transform.scale = glm::vec3(1.0f);
        instance->transform.reset();
        collection.push_back(std::move(instance));
    }

    return  collection;
}

std::unique_ptr<ModelInstance> ModelInstanceFactory::create( const domain::v1::ImportedAsset& asset,Mesh* mesh) const
{
    auto instance = std::make_unique<ModelInstance>();
    instance->id = utils::generateGuid();
    instance->name = asset.name;
    instance->modelHash = asset.Id;
    instance->transform.position = glm::vec3(0.0f);
    instance->transform.rotation = glm::vec3(0.0f);
    instance->transform.scale = glm::vec3(1.0f);
    instance->transform.reset();
    return instance;
}

 
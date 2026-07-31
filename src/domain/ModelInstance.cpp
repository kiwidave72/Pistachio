#include "ModelInstance.h"

 #include "core/GuidUtils.h"

namespace domain::v1 {

    ModelInstance::ModelInstance()
        : id(utils::generateGuid())
    {
    }

    ModelInstance::ModelInstance(std::string existingId)
        : id(std::move(existingId))
    {
    }

}
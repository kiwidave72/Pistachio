// domain/RaycastHit.h
#pragma once
#include <glm/glm.hpp>
#include <string>
#include <memory>
class RenderModel;
namespace domain::v1 { class Model; }
struct RaycastHit
{
    bool hit = false;
    std::string instanceId;
    RenderModel* renderModel = nullptr;
    std::shared_ptr<domain::v1::Model> model = nullptr;
    glm::vec3 point, normal;
    float distance = std::numeric_limits<float>::max();
    uint32_t triangleIndex = 0;
    std::string buildPlateId;
    bool isPlateHit = false;
};
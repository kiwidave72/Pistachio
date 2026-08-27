#pragma once
#include "domain/RenderCameraContext.h"
#include "domain/Transform.h"
#include "domain/RaycastHit.h"
#include "domain/Model.h"
#include "ports/IModelRenderStrategy.h"
#include "ports/IDrawable.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <chrono>
#include <memory>
#include <string>

class RenderModel final : public IDrawable
{
public:
    std::shared_ptr<domain::v1::Model> model;
    GLuint vao = 0, vbo = 0, ebo = 0;
    uint32_t indexCount = 0;
    std::string instanceId;

    RenderModel();
    ~RenderModel() override;

    bool raycastBoundsOnly(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::mat4 modelMatrix, bool debug = false) const;
    bool create(std::shared_ptr<domain::v1::Model> sourceModel, glm::vec2& center, bool isBuildPlateModel = false);
    void createVertixBuffer();
    glm::mat4 getBoundingBoxMatrix(const Transform& transform, glm::vec2 layoutOffset) const;

    void render(glm::vec3 color, const Transform& transform, GLuint shader, glm::mat4 view, glm::mat4 proj,
        glm::vec3 camPos, glm::vec2 center, float ghostFactor = 1.0f, bool isPlate = false) const;

    // IDrawable
    void draw(const IModelRenderStrategy& strategy, const RenderDrawState& drawState,
        const domain::v1::RenderCameraContext& cameraContext, const Transform& transform,
        GLuint shader, glm::vec2 center) override;

    glm::mat4 getModelMatrix(const Transform& transform, glm::vec2 center, bool isPlate = false) const;
    bool raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, RaycastHit& outHit, glm::mat4 modelMatrix) const;

private:
    bool intersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t, float& u, float& v) const;

    // Throttles the [RenderModel::draw] debug dump to once per second per
    // instance, instead of once per frame -- draw() is called once per
    // model per frame (every instance on every plate, in both
    // editable_scene and multi_plate_scene), so unthrottled it drowns
    // the console. min() so the very first draw() call always logs.
    std::chrono::steady_clock::time_point m_lastDrawDebugLogTime = std::chrono::steady_clock::time_point::min();
};
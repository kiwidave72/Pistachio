#pragma once
#include "domain/AnimatedModel.h"
#include "domain/RenderModel.h"
#include "domain/BuildPlate.h"
#include "domain/ModelCache.h"
#include "ports/ModelRenderStrategies.h"
#include "core/IEasedTransition.h"
#include "core/CubicEasedTransition.h"

#include <cstdio>


class EditableSceneLayout
{
public:
    void setActiveBuildPlate(domain::v1::BuildPlate* plate, domain::v1::ModelCache& cache)
    {
        m_instanceModels.clear();
        m_plateModel.reset();
        m_activePlate = plate;
        m_plateOffset = glm::vec2(0.0f);
        m_instanceOffset = glm::vec2(0.0f);
        if (!plate) return;

        // Both PlateModelRenderStrategy and StandardModelRenderStrategy
        // (ports/ModelRenderStrategies.h) build the model matrix as
        // worldPosMat * axisFix * rotScale, i.e. worldPos (which is where
        // "center" lands) is the OUTERMOST translation -- applied *after*
        // axisFix has already rotated whatever it's multiplying. axisFix
        // is a -90 degree rotation about X, which maps local (x,y,z) ->
        // (x, z, -y): local Y becomes NEGATIVE world Z.
        //
        // That rotation only ever touches this model's own local mesh
        // vertices, not the translation itself -- so it matters here in
        // two genuinely different ways for these two offsets:
        //
        //  - m_plateOffset centers the PLATE MESH'S OWN vertices (its
        //    local bounds go through axisFix same as any other geometry).
        //    Its local Y ends up as -world Z, so undoing that needs a
        //    matching sign flip: offset.y = +(min.y+max.y)/2, not the
        //    naive -(min.y+max.y)/2 that would centre a same-signed axis.
        //    Confirmed empirically: with a plain "-midpoint" on both axes,
        //    X centered correctly but Z came out -575..-225 instead of
        //    -175..175 -- exactly the sign-flip signature.
        //
        //  - m_instanceOffset converts instances' PLACEMENT coordinate
        //    (transform.position, in the domain "front-left corner = local
        //    (0,0), back-right corner = local (bedSize,bedSize), Z=height"
        //    convention) into world space. StandardModelRenderStrategy::
        //    computeModelMatrix() converts domain space into world space
        //    the same way axisFix converts mesh vertices (domain Y becomes
        //    world -Z): worldPos.z = center.y - position.y. So centering
        //    needs center.y = +halfBedSize here too, same sign-flip
        //    reasoning as m_plateOffset above, not -halfBedSize. (X is
        //    unaffected by the rotation either way: center.x = -halfBedSize
        //    is correct on that axis for both offsets.)
        //
        // tl;dr: these are two different offsets for two different
        // reasons, not the same value reused -- do not merge them back
        // into one "center" again without re-deriving both.
        if (plate->buildPlateModel && plate->buildPlateModel->mesh)
        {
            const auto& b = plate->buildPlateModel->mesh->bounds;

            m_plateOffset = glm::vec2(
                -(b.min.x + b.max.x) * 0.5f,
                (b.min.y + b.max.y) * 0.5f);

            float halfBedW = (b.max.x - b.min.x) * 0.5f;
            float halfBedD = (b.max.y - b.min.y) * 0.5f;
            m_instanceOffset = glm::vec2(-halfBedW, halfBedD);

            // Drop the plate down by its own thickness (mesh's local max
            // Z) so its top surface lands at world Y=0, matching where
            // instances resting on the bed (domain Z=0) actually render
            // via StandardModelRenderStrategy -- otherwise the plate's
            // top surface sits at world Y=(max Z) and every instance ends
            // up sunk into it by that amount. See PlateModelRenderStrategy
            // ::setHeightOffset in ports/ModelRenderStrategies.h.
            m_plateStrategy->setHeightOffset(-b.max.z);
        }

        if (plate->buildPlateModel)
        {
            auto rm = std::make_shared<RenderModel>();
            if (rm->create(plate->buildPlateModel, m_plateOffset, true))
            {
                m_plateModel = std::make_shared<AnimatedModel>(rm, m_plateStrategy,
                    std::make_unique<core::CubicEasedTransition>(), std::make_unique<core::CubicEasedTransition>());
                m_plateModel->setImmediateTransform(Transform{});
                // Slightly cool-toned near-white for a "silver" read.
                // Brighter than the previous flat 0.8 grey since the plate
                // is mostly viewed at grazing angles relative to the
                // camera-forward light direction (RenderModel::draw()),
                // landing closer to the shader's ambient floor and reading
                // as dark otherwise -- compensating with a brighter base
                // color rather than changing the shared lighting math.
                m_plateModel->setImmediateColor(glm::vec3(0.92f, 0.93f, 0.95f));
            }
        }

        printf("[EditableSceneLayout] setActiveBuildPlate: plate=%s, modelInstances.size()=%zu, "
            "plateOffset=(%.2f,%.2f) instanceOffset=(%.2f,%.2f)\n",
            plate->Id.c_str(), plate->modelInstances.size(),
            m_plateOffset.x, m_plateOffset.y, m_instanceOffset.x, m_instanceOffset.y);

        int skippedNullInstance = 0, skippedNoModel = 0, skippedNoMesh = 0, skippedCreateFailed = 0, added = 0;

        for (auto& instance : plate->modelInstances)
        {
            if (!instance) { skippedNullInstance++; continue; }

            auto model = cache.getModel(instance->modelHash);
            if (!model)
            {
                skippedNoModel++;
                printf("[EditableSceneLayout]   instance %s: modelHash %s not found in cache\n",
                    instance->id.c_str(), instance->modelHash.c_str());
                continue;
            }
            if (!model->mesh)
            {
                skippedNoMesh++;
                printf("[EditableSceneLayout]   instance %s: model %s has no mesh\n",
                    instance->id.c_str(), model->Id.c_str());
                continue;
            }

            auto rm = std::make_shared<RenderModel>();
            if (!rm->create(model, m_instanceOffset))
            {
                skippedCreateFailed++;
                printf("[EditableSceneLayout]   instance %s: RenderModel::create() failed for model %s\n",
                    instance->id.c_str(), model->Id.c_str());
                continue;
            }
            rm->instanceId = instance->id;

            auto anim = std::make_shared<AnimatedModel>(rm, m_standardStrategy,
                std::make_unique<core::CubicEasedTransition>(), std::make_unique<core::CubicEasedTransition>());
            anim->setImmediateTransform(instance->transform);
            anim->setImmediateColor(instance->color);
            m_instanceModels.push_back(anim);
            added++;

            printf("[EditableSceneLayout]   instance %s: added (local pos=%.2f,%.2f,%.2f mesh verts=%zu)\n",
                instance->id.c_str(), instance->transform.position.x, instance->transform.position.y,
                instance->transform.position.z, model->mesh->vertices.size());
        }

        printf("[EditableSceneLayout] setActiveBuildPlate: added=%d, skipped(nullInstance=%d, noModel=%d, noMesh=%d, createFailed=%d)\n",
            added, skippedNullInstance, skippedNoModel, skippedNoMesh, skippedCreateFailed);
    }

    void tick(float dt)
    {
        if (m_plateModel) m_plateModel->tick(dt);
        for (auto& m : m_instanceModels) m->tick(dt);
    }

    void render(GLuint shader, const domain::v1::RenderCameraContext& cameraContext)
    {
        if (m_plateModel) m_plateModel->render(shader, cameraContext, m_plateOffset);
        for (auto& m : m_instanceModels) m->render(shader, cameraContext, m_instanceOffset);
    }

private:
    domain::v1::BuildPlate* m_activePlate = nullptr;
    std::shared_ptr<AnimatedModel> m_plateModel;
    std::vector<std::shared_ptr<AnimatedModel>> m_instanceModels;

    // See the long comment in setActiveBuildPlate() -- these are
    // deliberately different values for different reasons, not the same
    // offset reused.
    glm::vec2 m_plateOffset{ 0.0f };      // centers the plate mesh's own (rotated) vertices at world origin
    glm::vec2 m_instanceOffset{ 0.0f };   // converts instances' corner-origin placement coords -> bed-centered world coords

    std::shared_ptr<IModelRenderStrategy> m_standardStrategy = std::make_shared<StandardModelRenderStrategy>();
    std::shared_ptr<PlateModelRenderStrategy> m_plateStrategy = std::make_shared<PlateModelRenderStrategy>();
};
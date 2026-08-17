#include "adapters/plugins/ToolpathEnginePlugin/ImportPhase.h"
#include "ports/IConfigPort.h"   

#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

namespace kinetica {

    std::vector<domain::v1::UnifiedGeometry> ImportPhase::run(domain::v1::BuildPlate* buildPlate)
    {

        std::vector<domain::v1::UnifiedGeometry> result;
        if (!buildPlate) return result;

        // No bed-corner offset needed here: instance->transform.position
        // is already written in bed space by arrangeBuildPlate() (front-
        // left corner = local (0,0), back-right = local (bedSize,bedSize),
        // matching "bed space" this phase is producing). Adding
        // +bedSize/2 on top of that double-shifts every part -- a part
        // correctly centered at (175,175) was landing at (350,350), the
        // far corner, not just in the debug visualiser but in the actual
        // generated toolpath.

        for (auto& instance : buildPlate->modelInstances)
        {
            if (!instance) continue;

            auto model = m_modelCache.getModel(instance->modelHash);
            if (!model || !model->mesh)
            {
                printf("[ImportPhase] WARNING: no model/mesh for instance %s (hash %s) -- skipped\n",
                    instance->id.c_str(), instance->modelHash.c_str());
                continue;
            }

            glm::mat4 combined = instance->transform.matrix();

            domain::v1::UnifiedGeometry geom;
            geom.modelInstanceId = instance->id;
            geom.vertices.reserve(model->mesh->vertices.size());

            domain::v1::BoundingBox bounds;
            bool first = true;

            for (const auto& v : model->mesh->vertices)
            {
                glm::vec3 bedSpacePos = glm::vec3(combined * glm::vec4(v.position, 1.0f));

                domain::v1::Vertex outVertex;
                outVertex.position = bedSpacePos;
                outVertex.normal = glm::normalize(glm::mat3(combined) * v.normal);
                geom.vertices.push_back(outVertex);

                if (first) { bounds.min = bounds.max = bedSpacePos; first = false; }
                else
                {
                    bounds.min = glm::min(bounds.min, bedSpacePos);
                    bounds.max = glm::max(bounds.max, bedSpacePos);
                }
            }

            geom.indices = model->mesh->indices;
            geom.bounds = bounds;

            printf("[ImportPhase] instance %s: %zu vertices, bounds min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f)\n",
                instance->id.c_str(), geom.vertices.size(),
                geom.bounds.min.x, geom.bounds.min.y, geom.bounds.min.z,
                geom.bounds.max.x, geom.bounds.max.y, geom.bounds.max.z);

            result.push_back(std::move(geom));
        }



        return result;
    }

} // namespace kinetica
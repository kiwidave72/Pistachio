#pragma once
#include "domain/ModelCache.h"   // domain::v1::BoundingBox
#include <glm/glm.hpp>

namespace domain::v1 {

    // -----------------------------------------------------------------------
    // Derives the two world-space offsets needed to render one build plate
    // + its instances correctly, given only that plate's mesh's own local
    // bounds. Pulled out of EditableSceneLayout::setActiveBuildPlate (see
    // the long comment still living there) so MultiPlateSceneLayout can
    // reuse the exact same, empirically-verified axisFix sign-flip math per
    // plate instead of re-deriving it -- do not re-derive these independently,
    // see that comment for the full "why" of each field.
    //
    // Both offsets are returned RELATIVE TO PLATE-LOCAL SPACE (i.e. as if
    // this plate sat at world origin). A caller placing this plate at some
    // other world-space center (e.g. a grid cell) adds that center on top:
    //   plateRenderCenter    = worldCenter + offsets.plateOffset;
    //   instanceRenderCenter = worldCenter + offsets.instanceOffset;
    // -- valid because these offsets are pure translations applied by
    // IModelRenderStrategy::computeModelMatrix() (center.x/center.y are
    // just added to worldPos there), so they compose additively.
    // -----------------------------------------------------------------------
    struct PlateOffsets
    {
        glm::vec2 plateOffset{ 0.0f };      // centers the plate mesh's own (rotated) vertices at plate-local origin
        glm::vec2 instanceOffset{ 0.0f };   // converts instances' corner-origin placement coords -> bed-centered plate-local coords
        float plateHeightOffset = 0.0f;     // world Y to drop the plate mesh by so its top surface lands at Y=0 (see PlateModelRenderStrategy::setHeightOffset)
    };

    inline PlateOffsets computePlateOffsets(const BoundingBox& plateMeshBounds)
    {
        PlateOffsets o;

        o.plateOffset = glm::vec2(
            -(plateMeshBounds.min.x + plateMeshBounds.max.x) * 0.5f,
            (plateMeshBounds.min.y + plateMeshBounds.max.y) * 0.5f);

        float halfBedW = (plateMeshBounds.max.x - plateMeshBounds.min.x) * 0.5f;
        float halfBedD = (plateMeshBounds.max.y - plateMeshBounds.min.y) * 0.5f;
        o.instanceOffset = glm::vec2(-halfBedW, halfBedD);

        o.plateHeightOffset = -plateMeshBounds.max.z;

        return o;
    }

} // namespace domain::v1
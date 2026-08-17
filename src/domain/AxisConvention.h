#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// -----------------------------------------------------------------------
// AxisConvention.h
//
// Single source of truth for the domain-space (Z-up, matching 3D-printing/
// CAD mesh authoring and the JSON schema) <-> engine world-space (Y-up)
// axis conversion. Before this file existed, the same -90 degree rotation
// about X was independently hand-typed in three places
// (ports/ModelRenderStrategies.h x2, domain/RenderModel.cpp), kept "in
// sync" only by a comment telling people to keep them in sync -- and when
// DebugComparisonGLRender.cpp needed the INVERSE of this rotation, it was
// hand-derived a third way, got the sign wrong twice in a row (a rotation
// is not generally self-inverse, only the transpose is -- see
// engineToDomain() below), and only agreed with the other three by luck
// of matching numbers, not by sharing code.
//
// Everything that needs this conversion -- mesh transforms, camera
// conversion for views that deliberately don't transform their geometry
// (DebugComparisonGLRender) -- should use these functions instead of
// re-deriving the matrix or its inverse locally.
// -----------------------------------------------------------------------

namespace domain::v1 {

    // Domain (Z-up) -> Engine (Y-up): local(x,y,z) -> world(x,z,-y).
    // Applied to mesh vertex/normal data via the model matrix in
    // StandardModelRenderStrategy / PlateModelRenderStrategy / RenderModel
    // ::getModelMatrix().
    inline const glm::mat4& axisFixMatrix()
    {
        static const glm::mat4 m = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0));
        return m;
    }

    inline glm::vec3 domainToEngine(const glm::vec3& v)
    {
        return glm::vec3(v.x, v.z, -v.y);
    }

    // Engine (Y-up) -> Domain (Z-up): the exact inverse. For a rotation,
    // inverse == transpose, NOT the same permutation run "backwards" by
    // eye -- axisFixMatrix() is a -90 degree rotation, and only a 180
    // degree rotation is its own inverse. Given the matrix form of
    // domainToEngine() above,
    //   R = [[1,0,0],[0,0,1],[0,-1,0]]
    // the correct inverse is R^T = [[1,0,0],[0,0,-1],[0,1,0]], i.e.
    // world(x,y,z) -> local(x,-z,y). Used by DebugComparisonGLRender to
    // convert the shared engine-space camera back into the native Z-up
    // space its (deliberately untransformed) geometry lives in.
    inline glm::vec3 engineToDomain(const glm::vec3& v)
    {
        return glm::vec3(v.x, -v.z, v.y);
    }

} // namespace domain::v1
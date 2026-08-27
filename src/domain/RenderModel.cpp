#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "domain/DataContext.h"

#include "domain/RenderModel.h"
#include "domain/AxisConvention.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
    // Same lift/light constants used by the other GL mesh renderers
    // (StlPreviewRenderer, BuildPlateRenderer) — kept identical so parts
    // and the build plate are lit consistently across viewports.
    constexpr glm::vec3 kLightDir{ -0.4f, -0.8f, -0.4f };

    // glad's function-pointer table is a set of globals private to whichever
    // module (DLL) it's statically linked into. pistachio_core.dll links its
    // own copy of glad, and nothing in this module had ever called
    // gladLoadGLLoader() before RenderModel::createVertixBuffer() made this
    // module's first raw GL call (glGenVertexArrays et al) during plugin
    // load — so every pointer in that table was still null and the call
    // crashed. Same lazy-load-once guard used by StlPreviewRenderer::ensureGl()
    // / CameraGizmoGLRender / BuildPlateRenderer, just scoped to this file.
    bool ensureGladLoadedForThisModule()
    {
        static bool ready = false;
        if (ready) return true;

#ifdef _WIN32
        auto loader = [](const char* name) -> void*
            {
                void* p = (void*)wglGetProcAddress(name);
                if (!p)
                {
                    static HMODULE gl32 = LoadLibraryA("opengl32.dll");
                    if (gl32) p = (void*)GetProcAddress(gl32, name);
                }
                return p;
            };

        if (!gladLoadGLLoader((GLADloadproc)loader))
        {
            printf("[RenderModel] gladLoadGLLoader failed\n");
            return false;
        }
#endif

        if (!glad_glGenBuffers)
        {
            printf("[RenderModel] GL not available after gladLoadGLLoader\n");
            return false;
        }

        ready = true;
        return true;
    }
}

// -----------------------------------------------------------------------
// RenderModel Implementation � lifted from BuildPlateRenderer.cpp,
// unchanged except draw() (the IDrawable override), added new alongside
// the existing render() (which is untouched � BuildPlateRenderer keeps
// working exactly as before, once it's updated to include this header
// instead of declaring RenderModel itself).
// -----------------------------------------------------------------------

RenderModel::RenderModel() {}

RenderModel::~RenderModel()
{
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}

bool RenderModel::raycastBoundsOnly(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, glm::mat4 modelMatrix, bool debug) const
{
    if (!model || !model->mesh)
    {
        if (debug) printf("[RenderModel::raycastBoundsOnly] instanceId=%s NO model/mesh -- always misses\n", instanceId.c_str());
        return false;
    }

    const auto& b = model->mesh->bounds;
    glm::vec3 center = (b.max + b.min) * 0.5f;
    float radius = glm::length(b.max - b.min) * 0.5f;

    glm::vec4 worldCenter4 = modelMatrix * glm::vec4(center, 1.0f);
    glm::vec3 worldCenter = glm::vec3(worldCenter4) / worldCenter4.w;

    glm::vec3 scale(glm::length(glm::vec3(modelMatrix[0])), glm::length(glm::vec3(modelMatrix[1])), glm::length(glm::vec3(modelMatrix[2])));
    float worldRadius = radius * (std::max)({ scale.x, scale.y, scale.z });

    glm::vec3 oc = rayOrigin - worldCenter;
    float b2 = glm::dot(oc, rayDirection);
    float c = glm::dot(oc, oc) - worldRadius * worldRadius;
    float discriminant = b2 * b2 - c;

    if (debug)
    {
        printf("[RenderModel::raycastBoundsOnly] instanceId=%s localBounds(min=%.2f,%.2f,%.2f max=%.2f,%.2f,%.2f) "
            "localRadius=%.2f scale=(%.3f,%.3f,%.3f) worldCenter=(%.2f,%.2f,%.2f) worldRadius=%.2f discriminant=%.2f -> %s\n",
            instanceId.c_str(), b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z,
            radius, scale.x, scale.y, scale.z,
            worldCenter.x, worldCenter.y, worldCenter.z, worldRadius, discriminant,
            (discriminant >= 0.0f) ? "PASS" : "FAIL");
    }

    return discriminant >= 0.0f;
}

void RenderModel::createVertixBuffer()
{
    if (!ensureGladLoadedForThisModule()) return;

    std::shared_ptr<domain::v1::Mesh> mesh = model->mesh;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh->vertices.size() * sizeof(domain::v1::Vertex), mesh->vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indices.size() * sizeof(uint32_t), mesh->indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(domain::v1::Vertex), (void*)offsetof(domain::v1::Vertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(domain::v1::Vertex), (void*)offsetof(domain::v1::Vertex, normal));

    glBindVertexArray(0);

    indexCount = (uint32_t)mesh->indices.size();
}

bool RenderModel::create(std::shared_ptr<domain::v1::Model> sourceModel, glm::vec2& center, bool isBuildPlateModel)
{
    if (!sourceModel->mesh)
        return false;

    model = std::make_shared<domain::v1::Model>();
    model->fileName = sourceModel->fileName;
    model->fileLocation = sourceModel->fileLocation;
    model->label = sourceModel->label;
    model->Id = sourceModel->Id;
    model->mesh = sourceModel->mesh;
    createVertixBuffer();

    return true;
}

// -----------------------------------------------------------------------
// getModelMatrix -- mirrors StandardModelRenderStrategy / PlateModelRenderStrategy
// in ports/ModelRenderStrategies.h. Both now share the same
// domain::v1::axisFixMatrix() (see domain/AxisConvention.h) instead of
// separately hand-typing the rotation, so they can't drift apart the way
// they used to be able to.
// -----------------------------------------------------------------------
glm::mat4 RenderModel::getModelMatrix(const Transform& transform, glm::vec2 center, bool isPlate) const
{
    glm::mat4 axisFix = domain::v1::axisFixMatrix();

    if (isPlate)
    {
        glm::vec3 worldPos(center.x + transform.position.x, 0.0f, center.y + transform.position.z);
        glm::mat4 worldPosMat = glm::translate(glm::mat4(1.0f), worldPos);
        return worldPosMat * axisFix;
    }

    // See StandardModelRenderStrategy::computeModelMatrix in
    // ports/ModelRenderStrategies.h for why this isn't a straight pass-
    // through of transform.position.y/.z -- transform.position is
    // domain-space (Z = height, X/Y = footprint), and needs the same
    // axis conversion mesh vertices get via axisFix, not a raw copy into
    // world Y/Z.
    glm::vec3 worldPos(center.x + transform.position.x, transform.position.z, center.y - transform.position.y);
    glm::mat4 worldPosMat = glm::translate(glm::mat4(1.0f), worldPos);

    glm::mat4 rotScale(1.0f);
    rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.x), glm::vec3(1, 0, 0));
    rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
    rotScale = glm::rotate(rotScale, glm::radians(transform.rotation.z), glm::vec3(0, 0, 1));
    rotScale = glm::scale(rotScale, transform.scale);

    return worldPosMat * axisFix * rotScale;
}

// -----------------------------------------------------------------------
glm::mat4 RenderModel::getBoundingBoxMatrix(const Transform& transform, glm::vec2 layoutOffset) const
{
    glm::mat4 modelMatrix = getModelMatrix(transform, layoutOffset, false);
    if (!model || !model->mesh) return modelMatrix;

    const auto& bounds = model->mesh->bounds;

    // BoundingBoxRenderer draws a unit (-0.5..0.5) cube — translate to the
    // mesh-space bounds center and scale to the bounds size so the cube
    // wraps the mesh exactly once transformed by modelMatrix.
    glm::mat4 boxTransform = glm::translate(glm::mat4(1.0f), bounds.center());
    boxTransform = glm::scale(boxTransform, bounds.size());

    return modelMatrix * boxTransform;
}

// -----------------------------------------------------------------------
void RenderModel::render(glm::vec3 color, const Transform& transform, GLuint shader, glm::mat4 view, glm::mat4 proj,
    glm::vec3 camPos, glm::vec2 center, float ghostFactor, bool isPlate) const
{
    if (!ensureGladLoadedForThisModule()) return;
    if (!vao || indexCount == 0) return;

    glm::mat4 modelMatrix = getModelMatrix(transform, center, isPlate);

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3f(glGetUniformLocation(shader, "uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniform3f(glGetUniformLocation(shader, "uLightDir"), kLightDir.x, kLightDir.y, kLightDir.z);
    glUniform3f(glGetUniformLocation(shader, "uBaseColor"), color.r, color.g, color.b);
    glUniform1f(glGetUniformLocation(shader, "uGhostFactor"), ghostFactor);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// -----------------------------------------------------------------------
// IDrawable::draw � the strategy-driven counterpart of render(), used by
// AnimatedModel so animated/gizmo-driven draws share the same strategy
// objects (StandardModelRenderStrategy / PlateModelRenderStrategy) as the
// rest of the render pipeline instead of duplicating the matrix math here.
// -----------------------------------------------------------------------
void RenderModel::draw(const IModelRenderStrategy& strategy, const RenderDrawState& drawState,
    const domain::v1::RenderCameraContext& cameraContext, const Transform& transform,
    GLuint shader, glm::vec2 center)
{
    if (!ensureGladLoadedForThisModule()) return;

    if (!vao || indexCount == 0)
    {
        printf("[RenderModel::draw] bailing early: vao=%u indexCount=%u (instanceId=%s)\n",
            vao, indexCount, instanceId.c_str());
        return;
    }

    glm::mat4 modelMatrix = strategy.computeModelMatrix(transform, center);

    // Throttled to once per second per instance -- see
    // m_lastDrawDebugLogTime in RenderModel.h. draw() runs once per model
    // per frame, so logging every call here floods the console; a
    // periodic sample is still enough to catch a scale/position data
    // problem without a debugger.
    auto now = std::chrono::steady_clock::now();
    bool shouldLog = (now - m_lastDrawDebugLogTime) >= std::chrono::seconds(1);
    if (shouldLog) m_lastDrawDebugLogTime = now;

    if (shouldLog)
    {
        // One-shot-per-call dump of where this is actually being placed, so we
        // can tell a scale/position data problem apart from a GL state problem
        // without a debugger. modelMatrix[3] is the translation column; the
        // length of each basis column tells us the effective scale per axis.
        // Also dumps this model's own local mesh bounds (min/max, before any
        // transform/offset is applied) plus the offset ("center") passed in,
        // and the camera's position/target -- enough together to tell whether
        // an object is sitting where expected relative to where the camera is
        // actually looking.
        glm::vec3 worldPos(modelMatrix[3]);
        glm::vec3 effScale(
            glm::length(glm::vec3(modelMatrix[0])),
            glm::length(glm::vec3(modelMatrix[1])),
            glm::length(glm::vec3(modelMatrix[2])));

        glm::vec3 meshMin(0.0f), meshMax(0.0f);
        if (model && model->mesh)
        {
            meshMin = model->mesh->bounds.min;
            meshMax = model->mesh->bounds.max;
        }

        // Four footprint corners (min/max Z of the mesh's own bounds, at each
        // XY corner) run through the exact same modelMatrix used to actually
        // render, so this is ground truth for where the mesh's real geometry
        // ends up in world space -- unlike worldPos above, which is only the
        // local-origin POINT's world position and says nothing about where
        // the mesh's actual footprint is once its own bounds (which may not
        // be centered on that origin, or even contain it) are accounted for.
        glm::vec3 c0 = glm::vec3(modelMatrix * glm::vec4(meshMin.x, meshMin.y, meshMin.z, 1.0f)); // min,min
        glm::vec3 c1 = glm::vec3(modelMatrix * glm::vec4(meshMax.x, meshMin.y, meshMin.z, 1.0f)); // max,min
        glm::vec3 c2 = glm::vec3(modelMatrix * glm::vec4(meshMin.x, meshMax.y, meshMin.z, 1.0f)); // min,max
        glm::vec3 c3 = glm::vec3(modelMatrix * glm::vec4(meshMax.x, meshMax.y, meshMin.z, 1.0f)); // max,max

        printf("[RenderModel::draw] instanceId=%s vao=%u indexCount=%u transform(pos=%.2f,%.2f,%.2f rot=%.2f,%.2f,%.2f scale=%.2f,%.2f,%.2f) "
            "center=%.2f,%.2f meshBounds(min=%.2f,%.2f,%.2f max=%.2f,%.2f,%.2f) "
            "worldPos=%.2f,%.2f,%.2f effScale=%.3f,%.3f,%.3f color=%.2f,%.2f,%.2f ghost=%.2f "
            "camPos=%.2f,%.2f,%.2f camTarget=%.2f,%.2f,%.2f "
            "worldCorners[minmin=%.2f,%.2f,%.2f maxmin=%.2f,%.2f,%.2f minmax=%.2f,%.2f,%.2f maxmax=%.2f,%.2f,%.2f]\n",
            instanceId.c_str(), vao, indexCount,
            transform.position.x, transform.position.y, transform.position.z,
            transform.rotation.x, transform.rotation.y, transform.rotation.z,
            transform.scale.x, transform.scale.y, transform.scale.z,
            center.x, center.y,
            meshMin.x, meshMin.y, meshMin.z, meshMax.x, meshMax.y, meshMax.z,
            worldPos.x, worldPos.y, worldPos.z,
            effScale.x, effScale.y, effScale.z,
            drawState.color.r, drawState.color.g, drawState.color.b, drawState.ghostFactor,
            cameraContext.camPos.x, cameraContext.camPos.y, cameraContext.camPos.z,
            cameraContext.target.x, cameraContext.target.y, cameraContext.target.z,
            c0.x, c0.y, c0.z, c1.x, c1.y, c1.z, c2.x, c2.y, c2.z, c3.x, c3.y, c3.z);
    }

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, glm::value_ptr(cameraContext.view));
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProj"), 1, GL_FALSE, glm::value_ptr(cameraContext.proj));
    glUniform3f(glGetUniformLocation(shader, "uCamPos"), cameraContext.camPos.x, cameraContext.camPos.y, cameraContext.camPos.z);

    // Camera-relative, not the old fixed kLightDir: a light direction
    // fixed in world space stays put in world space, so if the camera
    // orbits underneath or around an object, the light can end up lighting
    // the far/wrong side relative to whatever you're actually looking at
    // (the "shading mixed up" symptom). Blending the camera's own forward
    // direction with a strong downward bias keeps the light generally in
    // front of wherever you're looking (so surfaces facing the camera stay
    // lit) while still reading as "coming from above" regardless of orbit
    // angle -- effectively a lamp mounted above the camera, always pointed
    // roughly at whatever you're viewing.
    glm::vec3 camForward = glm::normalize(cameraContext.target - cameraContext.camPos);
    glm::vec3 lightDir = glm::normalize(camForward + glm::vec3(0.0f, -0.7f, 0.0f));
    glUniform3f(glGetUniformLocation(shader, "uLightDir"), lightDir.x, lightDir.y, lightDir.z);

    strategy.applyShaderUniforms(shader, drawState.color, drawState.ghostFactor);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// -----------------------------------------------------------------------
// Standard Moller-Trumbore ray/triangle intersection, in whatever space
// the three vertices are given in (raycast() below passes world-space
// verts, already transformed by modelMatrix).
// -----------------------------------------------------------------------
bool RenderModel::intersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, float& t, float& u, float& v) const
{
    constexpr float kEpsilon = 1e-6f;

    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDirection, edge2);
    float a = glm::dot(edge1, h);
    if (std::fabs(a) < kEpsilon) return false; // ray parallel to triangle

    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, edge1);
    v = f * glm::dot(rayDirection, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    t = f * glm::dot(edge2, q);
    return t > kEpsilon;
}

// -----------------------------------------------------------------------
bool RenderModel::raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, RaycastHit& outHit, glm::mat4 modelMatrix) const
{
    if (!model || !model->mesh) return false;

    const auto& mesh = *model->mesh;
    const size_t triangleCount = mesh.indices.size() / 3;

    bool found = false;
    float closestT = (std::numeric_limits<float>::max)();
    uint32_t closestTriangle = 0;
    glm::vec3 closestNormal(0.0f);

    for (size_t i = 0; i < triangleCount; ++i)
    {
        const glm::vec3& l0 = mesh.vertices[mesh.indices[i * 3 + 0]].position;
        const glm::vec3& l1 = mesh.vertices[mesh.indices[i * 3 + 1]].position;
        const glm::vec3& l2 = mesh.vertices[mesh.indices[i * 3 + 2]].position;

        glm::vec3 v0 = glm::vec3(modelMatrix * glm::vec4(l0, 1.0f));
        glm::vec3 v1 = glm::vec3(modelMatrix * glm::vec4(l1, 1.0f));
        glm::vec3 v2 = glm::vec3(modelMatrix * glm::vec4(l2, 1.0f));

        float t, u, v;
        if (!intersectTriangle(rayOrigin, rayDirection, v0, v1, v2, t, u, v)) continue;
        if (t >= closestT) continue;

        found = true;
        closestT = t;
        closestTriangle = (uint32_t)i;
        closestNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    }

    if (!found) return false;

    outHit.hit = true;
    outHit.renderModel = const_cast<RenderModel*>(this);
    outHit.model = model;
    outHit.point = rayOrigin + rayDirection * closestT;
    outHit.normal = closestNormal;
    outHit.distance = closestT;
    outHit.triangleIndex = closestTriangle;

    return true;
}
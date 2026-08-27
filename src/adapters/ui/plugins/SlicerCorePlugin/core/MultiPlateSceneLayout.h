#pragma once
#include "domain/AnimatedModel.h"
#include "domain/RenderModel.h"
#include "domain/BuildPlate.h"
#include "domain/ModelCache.h"
#include "domain/PlateOffsets.h"
#include "ports/ModelRenderStrategies.h"
#include "ports/I3DViewportGLRender.h"   // CameraState
#include "core/IEasedTransition.h"
#include "core/CubicEasedTransition.h"
#include "domain/RaycastHit.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

// -----------------------------------------------------------------------
// MultiPlateSceneLayout
//
// The multi-plate counterpart to EditableSceneLayout -- built the exact
// same way (AnimatedModel + RenderModel + IModelRenderStrategy, eased
// per-instance transitions), just laid out across a world-space grid
// instead of a single plate centered at the origin.
//
// Deliberately NOT built on top of the old (unused) slicer::SceneLayout
// in BuildPlateRenderer.h/.cpp -- that class predates AnimatedModel and
// the per-plate axisFix offset derivation entirely, and isn't wired into
// the current I3DViewportGLRender/ViewportController registry pattern.
// bestGrid() is the one piece of that class genuinely worth reusing (a
// clean, self-contained algorithm); it's reimplemented here rather than
// pulling in the whole legacy header.
//
// One PlateModelRenderStrategy PER PLATE (not shared, unlike
// EditableSceneLayout's single m_plateStrategy) since setHeightOffset()
// is instance state on that strategy -- plates with differently-sized
// plate meshes need their own height offset. StandardModelRenderStrategy
// is stateless and safe to share across every instance on every plate,
// same as EditableSceneLayout does.
// -----------------------------------------------------------------------
class MultiPlateSceneLayout
{
public:
    struct PlateEntry
    {
        std::string plateId;
        glm::vec2 worldCenter{ 0.0f };            // this plate's grid-cell center in world space
        glm::vec2 plateRenderCenter{ 0.0f };       // worldCenter + this plate's plateOffset -- passed into render()/raycast()
        glm::vec2 instanceRenderCenter{ 0.0f };    // worldCenter + this plate's instanceOffset

        std::shared_ptr<PlateModelRenderStrategy> plateStrategy;
        std::shared_ptr<AnimatedModel> plateModel;
        std::vector<std::shared_ptr<AnimatedModel>> instanceModels;

        // Per-plate ghost state -- see animateGhostOutExcept() below for
        // why this needs to be per-plate rather than one scene-wide value.
        float ghostFactor = 1.0f;
        float ghostFrom = 1.0f, ghostTo = 1.0f;
    };

    void createLayout(std::vector<domain::v1::BuildPlate*> buildPlates, domain::v1::ModelCache& cache)
    {
        m_plates.clear();

        int nItems = (int)buildPlates.size();
        int cols = 1, rows = 1;
        if (nItems > 0) bestGrid(nItems, 3, 1, cols, rows);

        float totalWidth = cols * m_itemSize + (cols - 1) * m_padding;
        float totalHeight = rows * m_itemSize + (rows - 1) * m_padding;
        float leftEdge = -totalWidth * 0.5f;
        float topEdge = totalHeight * 0.5f;

        int i = 0;
        for (auto* plate : buildPlates)
        {
            if (!plate) { i++; continue; }

            int col = i % cols;
            int row = i / cols;

            float colOffset = leftEdge + col * (m_itemSize + m_padding) + m_itemSize * 0.5f;
            float rowOffset = topEdge - row * (m_itemSize + m_padding) - m_itemSize * 0.5f;

            PlateEntry entry;
            entry.plateId = plate->Id;
            // Columns run along world Z (negated), rows along world X --
            // swapped from the "columns=X, rows=Z" convention this had
            // when the overview camera's yaw was 45 degrees. At yaw=0,
            // right = (sin(0),0,-cos(0)) = (0,0,-1) (see
            // computeOverviewCamera()), i.e. screen-right is world -Z,
            // not +X -- laying columns out along X (unchanged) would
            // make a single row of plates read as stacked front-to-back
            // instead of side-by-side. Negated so increasing column
            // index still moves rightward on screen (dot with right
            // increases with colOffset) rather than leftward.
            entry.worldCenter = glm::vec2(rowOffset, -colOffset);
            entry.plateRenderCenter = entry.worldCenter;
            entry.instanceRenderCenter = entry.worldCenter;
            entry.plateStrategy = std::make_shared<PlateModelRenderStrategy>();

            if (plate->buildPlateModel && plate->buildPlateModel->mesh)
            {
                const auto& b = plate->buildPlateModel->mesh->bounds;
                auto offsets = domain::v1::computePlateOffsets(b);

                entry.plateRenderCenter = entry.worldCenter + offsets.plateOffset;
                entry.instanceRenderCenter = entry.worldCenter + offsets.instanceOffset;
                entry.plateStrategy->setHeightOffset(offsets.plateHeightOffset);

                auto rm = std::make_shared<RenderModel>();
                if (rm->create(plate->buildPlateModel, entry.plateRenderCenter, true))
                {
                    entry.plateModel = std::make_shared<AnimatedModel>(rm, entry.plateStrategy,
                        std::make_unique<core::CubicEasedTransition>(), std::make_unique<core::CubicEasedTransition>());
                    entry.plateModel->setImmediateTransform(Transform{});
                    // Recolor immediately (no transition) to whichever
                    // plate is already tracked as active -- avoids a
                    // one-frame flash of the default silver before
                    // applyActivePlateHighlight() would otherwise catch
                    // up to it.
                    bool isActive = !m_activePlateId.empty() && entry.plateId == m_activePlateId;
                    entry.plateModel->setImmediateColor(isActive ? kActivePlateColor : kDefaultPlateColor);
                }
            }

            for (auto& instance : plate->modelInstances)
            {
                if (!instance) continue;

                auto model = cache.getModel(instance->modelHash);
                if (!model || !model->mesh) continue;

                auto rm = std::make_shared<RenderModel>();
                if (!rm->create(model, entry.instanceRenderCenter)) continue;
                rm->instanceId = instance->id;

                auto anim = std::make_shared<AnimatedModel>(rm, m_standardStrategy,
                    std::make_unique<core::CubicEasedTransition>(), std::make_unique<core::CubicEasedTransition>());
                anim->setImmediateTransform(instance->transform);
                anim->setImmediateColor(instance->color);
                entry.instanceModels.push_back(anim);
            }

            m_plates.push_back(std::move(entry));
            i++;
        }

        rebuildSceneExtents(totalWidth, totalHeight);
        applyGhostFactors();   // keep whatever fade was in progress -- a re-layout (Arrange, workspace load) shouldn't pop new geometry to full opacity mid-fade

        printf("[MultiPlateSceneLayout] createLayout: %zu plates, grid=%dx%d\n", m_plates.size(), cols, rows);
    }

    void tick(float dt)
    {
        if (m_ghostAnimating)
        {
            m_ghostTransition->tick(dt);
            float e = m_ghostTransition->progress();

            for (auto& p : m_plates)
                p.ghostFactor = glm::mix(p.ghostFrom, p.ghostTo, e);

            if (!m_ghostTransition->isAnimating())
            {
                for (auto& p : m_plates) p.ghostFactor = p.ghostTo;
                m_ghostAnimating = false;
            }
            applyGhostFactors();
        }

        for (auto& p : m_plates)
        {
            if (p.plateModel) p.plateModel->tick(dt);
            for (auto& m : p.instanceModels) m->tick(dt);
        }
    }

    void render(GLuint shader, const domain::v1::RenderCameraContext& cameraContext)
    {
        for (auto& p : m_plates)
        {
            if (p.plateModel) p.plateModel->render(shader, cameraContext, p.plateRenderCenter);
            for (auto& m : p.instanceModels) m->render(shader, cameraContext, p.instanceRenderCenter);
        }
    }

    // Parts always take priority over the bare plate underneath them --
    // same ordering as EditableSceneLayout::raycast(). On a hit,
    // RaycastHit::buildPlateId identifies which plate was hit -- the
    // multi-plate view has no other way to know, since every plate can
    // render identical geometry at different world offsets.
    //
    // One-shot debug logging: see debugLogNextRaycast() below.
    RaycastHit raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) const
    {
        bool debug = m_debugLogNextRaycast;
        m_debugLogNextRaycast = false;   // consumed regardless of outcome -- one shot per request

        if (debug)
            printf("[MultiPlateSceneLayout::raycast] origin=(%.2f,%.2f,%.2f) dir=(%.3f,%.3f,%.3f) plateCount=%zu\n",
                rayOrigin.x, rayOrigin.y, rayOrigin.z, rayDirection.x, rayDirection.y, rayDirection.z, m_plates.size());

        RaycastHit closest;
        closest.hit = false;
        closest.distance = (std::numeric_limits<float>::max)();

        for (auto& p : m_plates)
        {
            for (auto& m : p.instanceModels)
            {
                RaycastHit hit;
                if (m->raycast(rayOrigin, rayDirection, p.instanceRenderCenter, hit) && hit.distance < closest.distance)
                {
                    closest = hit;
                    closest.isPlateHit = false;
                    closest.buildPlateId = p.plateId;
                    if (closest.renderModel) closest.instanceId = closest.renderModel->instanceId;
                }
            }
        }
        if (closest.hit)   // a part was hit on some plate -- don't test plates
        {
            if (debug)
                printf("[MultiPlateSceneLayout::raycast] part hit -- plate=%s point=(%.2f,%.2f,%.2f) dist=%.2f\n",
                    closest.buildPlateId.c_str(), closest.point.x, closest.point.y, closest.point.z, closest.distance);
            return closest;
        }

        for (auto& p : m_plates)
        {
            if (!p.plateModel) continue;

            RaycastHit hit;
            bool got = p.plateModel->raycast(rayOrigin, rayDirection, p.plateRenderCenter, hit, debug);

            if (debug)
            {
                // modelMatrix[3] is ground truth for where the plate's
                // local origin actually ends up in world space -- the
                // exact same strategy call render()/raycast() use, not a
                // hand-derived estimate. Compare this against rayOrigin/
                // rayDir above to see directly whether the ray comes
                // anywhere near where the geometry actually is.
                glm::mat4 mm = p.plateStrategy->computeModelMatrix(Transform{}, p.plateRenderCenter);
                glm::vec3 mmPos(mm[3]);
                printf("[MultiPlateSceneLayout::raycast] plate=%s worldCenter=(%.1f,%.1f) plateRenderCenter=(%.1f,%.1f) "
                    "modelMatrixPos=(%.2f,%.2f,%.2f) hit=%d\n",
                    p.plateId.c_str(), p.worldCenter.x, p.worldCenter.y, p.plateRenderCenter.x, p.plateRenderCenter.y,
                    mmPos.x, mmPos.y, mmPos.z, got ? 1 : 0);
                if (got)
                    printf("[MultiPlateSceneLayout::raycast]   -> point=(%.2f,%.2f,%.2f) dist=%.2f\n",
                        hit.point.x, hit.point.y, hit.point.z, hit.distance);
            }

            if (got && hit.distance < closest.distance)
            {
                closest = hit;
                closest.isPlateHit = true;
                closest.buildPlateId = p.plateId;
            }
        }

        if (debug && !closest.hit)
            printf("[MultiPlateSceneLayout::raycast] NO HIT on any plate/instance\n");

        return closest;
    }

    // One-shot: the NEXT raycast() call prints per-plate diagnostics
    // (world/render centers, bounds+mesh test result, hit point) instead
    // of computing silently. Consumed on that next call whether or not
    // it hit anything. Set right before the one raycast() call you want
    // to inspect -- see the double-click debug logging in
    // SlicerCorePlugin::renderBuildPlate().
    void debugLogNextRaycast() const { m_debugLogNextRaycast = true; }

    // Unconditional dump of every plate's position data -- independent
    // of raycast() entirely, so it prints even if raycast() itself never
    // gets called or bails early. worldCenter/plateRenderCenter/
    // instanceRenderCenter as already used for layout and rendering,
    // plus the plate mesh's own local bounds (the same bounds
    // raycastBoundsOnly() reads) and instance count, so a genuinely
    // stale/duplicate/degenerate plate entry is visible directly rather
    // than inferred from a raycast miss.
    void debugDumpPlatePositions() const
    {
        printf("[MultiPlateSceneLayout] %zu plate(s) in layout:\n", m_plates.size());
        for (auto& p : m_plates)
        {
            printf("  plate=%s worldCenter=(%.2f,%.2f) plateRenderCenter=(%.2f,%.2f) instanceRenderCenter=(%.2f,%.2f) "
                "hasPlateModel=%d instanceCount=%zu ghostFactor=%.2f\n",
                p.plateId.c_str(), p.worldCenter.x, p.worldCenter.y,
                p.plateRenderCenter.x, p.plateRenderCenter.y,
                p.instanceRenderCenter.x, p.instanceRenderCenter.y,
                p.plateModel ? 1 : 0, p.instanceModels.size(), p.ghostFactor);

            if (p.plateModel)
            {
                glm::mat4 mm = p.plateStrategy->computeModelMatrix(Transform{}, p.plateRenderCenter);
                glm::vec3 mmPos(mm[3]);
                printf("    modelMatrixPos=(%.2f,%.2f,%.2f)\n", mmPos.x, mmPos.y, mmPos.z);
            }
        }
        printf("  sceneCenter=(%.2f,%.2f,%.2f) sceneRadius=%.2f\n", m_sceneCenter.x, m_sceneCenter.y, m_sceneCenter.z, m_sceneRadius);
    }

    bool isEmpty() const { return m_plates.empty(); }

    // -----------------------------------------------------------------
    // Active-plate highlight -- recolors whichever plate matches
    // plateId so it's visually obvious which one editable_scene is
    // currently showing (and which one double-clicking a DIFFERENT
    // plate will replace it with). Called every frame from
    // SlicerCorePlugin::renderBuildPlate(), pulling the id fresh from
    // EditableSceneLayout::getActivePlate() each time rather than being
    // pushed from every place that changes the active plate -- a no-op
    // most frames (see the early-out below), and it means there's no
    // call site that can forget to sync it and drift out of sync with
    // what's actually showing, the same class of bug as the camera/ghost
    // desync fixed earlier.
    // -----------------------------------------------------------------
    void setActivePlateId(const std::string& plateId)
    {
        if (plateId == m_activePlateId) return;
        m_activePlateId = plateId;
        applyActivePlateHighlight(0.3f);
    }

    // -----------------------------------------------------------------
    // Ghost fade -- fades plates/models in this view in or out. Used by
    // SlicerCorePlugin to animate this scene while handing off to/from
    // editable_scene -- see beginSwitchToEditable()/beginSwitchToMultiPlate()
    // there.
    // -----------------------------------------------------------------

    // Fades every plate toward `target` uniformly (1 = opaque, 0 =
    // invisible). Used returning to the overview, where there's no
    // single plate that should stay put -- every plate is arriving
    // together.
    void animateGhostAllTo(float target, float durationSeconds)
    {
        for (auto& p : m_plates) p.ghostFrom = p.ghostFactor;
        for (auto& p : m_plates) p.ghostTo = target;
        m_ghostTransition->start(durationSeconds);
        m_ghostAnimating = true;
    }

    // Fades every plate toward invisible EXCEPT keepPlateId, which stays
    // fully opaque. Used zooming from the overview into one specific
    // plate: that plate is where the camera is headed and where
    // editable_scene will pick up immediately after, so it shouldn't
    // disappear along with everything around it -- only the plates
    // being left behind fade out.
    void animateGhostOutExcept(const std::string& keepPlateId, float durationSeconds)
    {
        for (auto& p : m_plates)
        {
            p.ghostFrom = p.ghostFactor;
            p.ghostTo = (p.plateId == keepPlateId) ? 1.0f : 0.0f;
        }
        m_ghostTransition->start(durationSeconds);
        m_ghostAnimating = true;
    }

    // Sets every plate's ghost factor immediately, with no easing,
    // EXCEPT keepPlateId which is forced fully opaque -- used right
    // before this scene becomes the active renderer again, so every
    // OTHER plate fades back in from nothing while keepPlateId (usually
    // whichever plate editable_scene was just showing) is already
    // visible with no fade needed, since it was on screen a moment ago.
    // Pass an empty keepPlateId for the plain "every plate starts
    // invisible" case.
    void setImmediateGhostExcept(const std::string& keepPlateId, float othersGhost)
    {
        for (auto& p : m_plates)
        {
            float g = (p.plateId == keepPlateId) ? 1.0f : othersGhost;
            p.ghostFrom = p.ghostTo = p.ghostFactor = g;
        }
        m_ghostAnimating = false;
        applyGhostFactors();
    }

    bool isGhostAnimating() const { return m_ghostAnimating; }

    // World-space center to point the camera at for a plate by id -- used
    // to keep the shared ViewportController camera pointed at the right
    // spot while this view and editable_scene hand off to each other,
    // since the same plate sits at a different world position in each
    // (this view's grid cell vs editable_scene's world origin).
    //
    // Deliberately worldCenter, NOT plateRenderCenter. plateRenderCenter
    // is worldCenter + plateOffset, and plateOffset exists specifically
    // to CANCEL OUT the plate mesh's own local-space asymmetry (its
    // bounds aren't centered on its own local origin) -- axisFixMatrix()
    // maps local(x,y,z) -> world(x,z,-y) (domain/AxisConvention.h), so
    // working through that mapping, the mesh's local bounds-center ends
    // up landing EXACTLY at world (worldCenter.x, *, worldCenter.y) once
    // rendered, regardless of the plateOffset shift baked into the
    // transform's input. plateRenderCenter is an intermediate parameter
    // fed into that transform, not where the plate is; a previous pass
    // aimed the camera at plateRenderCenter instead and made the
    // overview framing worse, not better -- see
    // SlicerCorePlugin::beginSwitchToEditable()/beginSwitchToMultiPlate().
    bool tryGetPlateWorldCenter(const std::string& plateId, glm::vec2& outCenter) const
    {
        for (auto& p : m_plates)
        {
            if (p.plateId != plateId) continue;
            outCenter = p.worldCenter;
            return true;
        }
        return false;
    }

    // True while any plate is short of fully opaque -- used to decide
    // whether GL_BLEND needs to be enabled this frame, independent of
    // whether a transition is actively in flight (covers the one frame
    // a transition finishes on, and any plate left mid-fade).
    bool anyPlateGhosted() const
    {
        for (auto& p : m_plates) if (p.ghostFactor < 0.999f) return true;
        return false;
    }

    // Camera state that frames every plate in the grid -- used by the
    // view toggle to zoom out when switching into this view.
    //
    // fovYRadians/aspect MUST be the actual current viewport's values,
    // not guesses -- distance is solved by fitting the scene's REAL
    // oriented footprint (all 4 corners of its XZ bounding box,
    // projected onto this camera's own right/up axes) into the frustum,
    // not from a single circular "radius" the way this used to work.
    // That collapsed a wide-but-shallow layout (many plates in one row)
    // into one number dominated by the wide axis, which either doesn't
    // fit the short axis or -- what actually happened -- backs the
    // camera off far more than either axis needed, since the previous
    // formula (radius/sin(fovY/2)) implicitly assumed a roughly
    // circular/square footprint filling the narrower of the two FOV
    // directions. right/up here are computed directly from yaw/pitch
    // (not via buildCameraContext(), which also needs camPos -- circular,
    // since camPos is exactly what distance determines).
    CameraState computeOverviewCamera(float fovYRadians, float aspect) const
    {
        CameraState s;
        s.yaw = m_defaultYaw;
        s.pitch = m_defaultPitch;
        s.target = m_sceneCenter;

        glm::vec3 right(std::sin(m_defaultYaw), 0.0f, -std::cos(m_defaultYaw));
        glm::vec3 forward(
            -std::cos(m_defaultPitch) * std::cos(m_defaultYaw),
            -std::sin(m_defaultPitch),
            -std::cos(m_defaultPitch) * std::sin(m_defaultYaw));
        glm::vec3 up = -glm::normalize(glm::cross(right, forward));

        glm::vec3 corners[4] = {
            glm::vec3(m_sceneMinXZ.x, 0.0f, m_sceneMinXZ.y),
            glm::vec3(m_sceneMaxXZ.x, 0.0f, m_sceneMinXZ.y),
            glm::vec3(m_sceneMinXZ.x, 0.0f, m_sceneMaxXZ.y),
            glm::vec3(m_sceneMaxXZ.x, 0.0f, m_sceneMaxXZ.y),
        };

        float halfWidthNeeded = 0.0f, halfHeightNeeded = 0.0f;
        for (auto& c : corners)
        {
            glm::vec3 offset = c - s.target;
            halfWidthNeeded = (std::max)(halfWidthNeeded, std::fabs(glm::dot(offset, right)));
            halfHeightNeeded = (std::max)(halfHeightNeeded, std::fabs(glm::dot(offset, up)));
        }

        float halfFovY = fovYRadians * 0.5f;
        float distanceForHeight = halfHeightNeeded / std::tan(halfFovY);
        float distanceForWidth = halfWidthNeeded / (std::tan(halfFovY) * aspect);
        float fitDistance = (std::max)(distanceForHeight, distanceForWidth) * 1.15f;   // small margin off the frustum edge

        s.distance = glm::clamp(fitDistance, 400.0f, 3000.0f);
        return s;
    }

private:
    void applyGhostFactors()
    {
        for (auto& p : m_plates)
        {
            if (p.plateModel) p.plateModel->setGhostFactor(p.ghostFactor);
            for (auto& m : p.instanceModels) m->setGhostFactor(p.ghostFactor);
        }
    }

    void applyActivePlateHighlight(float durationSeconds)
    {
        for (auto& p : m_plates)
        {
            if (!p.plateModel) continue;
            glm::vec3 color = (p.plateId == m_activePlateId) ? kActivePlateColor : kDefaultPlateColor;
            if (durationSeconds > 0.0f) p.plateModel->animateColorTo(color, durationSeconds);
            else p.plateModel->setImmediateColor(color);
        }
    }

    void rebuildSceneExtents(float totalWidth, float totalHeight)
    {
        if (m_plates.empty())
        {
            m_sceneCenter = glm::vec3(0.0f);
            m_sceneRadius = 200.0f;
            m_sceneMinXZ = glm::vec2(-200.0f);
            m_sceneMaxXZ = glm::vec2(200.0f);
            return;
        }

        glm::vec2 minXZ(FLT_MAX), maxXZ(-FLT_MAX);
        for (auto& p : m_plates)
        {
            // worldCenter, NOT plateRenderCenter -- see
            // tryGetPlateWorldCenter() above: the plate mesh's own local
            // asymmetry is specifically cancelled out by plateOffset, so
            // it geometrically ends up centered at worldCenter once
            // rendered, not at plateRenderCenter (an intermediate
            // transform input).
            minXZ = glm::min(minXZ, p.worldCenter - glm::vec2(m_itemSize * 0.5f));
            maxXZ = glm::max(maxXZ, p.worldCenter + glm::vec2(m_itemSize * 0.5f));
        }

        glm::vec2 center2 = (minXZ + maxXZ) * 0.5f;
        m_sceneCenter = glm::vec3(center2.x, 0.0f, center2.y);
        m_sceneRadius = glm::length(maxXZ - minXZ) * 0.5f;
        m_sceneMinXZ = minXZ;
        m_sceneMaxXZ = maxXZ;
    }

    // Picks the cols x rows grid closest to the target aspect ratio for
    // nItems cells. Reimplementation of the (dead-code) slicer::SceneLayout
    // ::bestGrid() -- same algorithm, kept self-contained here rather than
    // pulling in BuildPlateRenderer.h.
    static void bestGrid(int nItems, int aspectW, int aspectH, int& bestCols, int& bestRows)
    {
        double targetRatio = static_cast<double>(aspectW) / aspectH;
        double bestDiff = (std::numeric_limits<double>::max)();

        bestCols = 1;
        bestRows = nItems;

        for (int cols = 1; cols <= nItems; ++cols)
        {
            int rows = static_cast<int>(std::ceil(static_cast<double>(nItems) / cols));
            double ratio = static_cast<double>(cols) / rows;
            double diff = std::fabs(ratio - targetRatio);

            if (diff < bestDiff)
            {
                bestDiff = diff;
                bestCols = cols;
                bestRows = rows;
            }
        }
    }

    std::vector<PlateEntry> m_plates;

    glm::vec3 m_sceneCenter{ 0.0f };
    float m_sceneRadius = 200.0f;
    glm::vec2 m_sceneMinXZ{ -200.0f }, m_sceneMaxXZ{ 200.0f };

    float m_itemSize = 350.0f;
    float m_padding = 100.0f;

    // Matches I3DViewportGLRender's own default*() conventions (radians) --
    // NOT the raw-degrees-into-a-radians-field bug in the old dead
    // slicer::SceneLayout::m_defaultPitch (45.0f used unconverted as
    // radians there, which is nonsense -- don't copy that).
    float m_defaultYaw = 0.0f;
    float m_defaultPitch = glm::radians(45.0f);

    bool m_ghostAnimating = false;
    std::unique_ptr<core::IEasedTransition> m_ghostTransition = std::make_unique<core::CubicEasedTransition>();

    std::string m_activePlateId;
    // Same near-white "silver" read as EditableSceneLayout's plate for
    // the default; a distinct blue accent for whichever one is active,
    // set via setActivePlateId(). inline (not constexpr) since GLM's
    // vec3 constructors aren't reliably constexpr-usable across the
    // versions this project might be pinned to.
    static inline const glm::vec3 kDefaultPlateColor{ 0.92f, 0.93f, 0.95f };
    static inline const glm::vec3 kActivePlateColor{ 0.35f, 0.60f, 0.95f };

    // One-shot flag for debugLogNextRaycast() -- mutable since raycast()
    // itself is logically const (it only queries, it just also needs to
    // consume this flag when set).
    mutable bool m_debugLogNextRaycast = false;

    std::shared_ptr<IModelRenderStrategy> m_standardStrategy = std::make_shared<StandardModelRenderStrategy>();
};
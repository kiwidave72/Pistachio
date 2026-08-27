#include "adapters/ui/plugins/UiModuleApi.h"
#include "adapters/ui/IconsFontAwesomeRegular.h"
#include "ports/ITaskProgressReporter.h"
#include "core/FileDialog.h"
#include "adapters/rendering/ViewportController.h"
#include "ports/IViewportRendererRegistry.h"

#include "EditableSceneGLRender.h"
#include "EditableSceneLayout.h"
#include "MultiPlateSceneGLRender.h"
#include "MultiPlateSceneLayout.h"

#include "core/Application.h"
#include "ports/IConfigPort.h"
#include "adapters/ui/ContributionRegistry.h"
#include "ports/Contributions.h"
#include "NavigationManager.h"

#include "adapters/ui/ImGuiHost.h"
#include "core/TaskRunner.h"
#include "core/GuidUtils.h"

#include "adapters/loaders/StepFileLoader.h"

#include <imgui.h>
#include <cstdio>

#include "FolderScanner.h"
#include "Project.h"
#include "StlPreviewRenderer.h"
#include "BuildPLateRenderer.h"

#include <map>
#include <unordered_map>
#include <unordered_set>

#include <algorithm>
#include "clipper2/clipper.h" 
#include "core/commands/ICommand.h"
#include "ModelInstanceFactory.h"
#include "domain/ModelCache.h"

#include "adapters/loaders/StlLoaderAdapter.h"
#include "CachedFolderScanner.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

using namespace Clipper2Lib;
using namespace adapters::scanning;
using namespace core::commands;



class SceneController {

public:
    SceneController();
    ~SceneController();


    void changeToMultiBuildPlateView(slicer::BuildPlateRenderer* buildPlateRender);

    void changeToSingleBuildPlateView(slicer::BuildPlateRenderer* buildPlateRender);

    void selectBuildPlate(domain::v1::Project* project, domain::v1::BuildPlate* selectBuildPlate);

    void UnselectedBuildPlates(domain::v1::Project* project);




private:

};


void SceneController::changeToSingleBuildPlateView(slicer::BuildPlateRenderer* buildPlateRender) {

    //buildPlateRender->getSceneLayout().createSingleLayout(true);

    //update the scenbounds - zooming??
    // 
    //move the camera to be inline with the build plate??c


}
class SnapshotCommand final : public ICommand
{
public:
    SnapshotCommand(domain::v1::WorkspaceStore& store,
        std::string name,
        std::function<void()> action)
        : m_store(store)
        , m_name(std::move(name))
        , m_action(std::move(action))
    {
    }

    const char* Name() const override { return m_name.c_str(); }

    void Do() override
    {
        if (!m_hasRun)
        {
            m_store.read([&](const domain::v1::Workspace& ws) { m_before = nlohmann::json(ws); });
            m_action();   // zero-arg — action already captured store/whatever it needs
            m_store.read([&](const domain::v1::Workspace& ws) { m_after = nlohmann::json(ws); });
            m_hasRun = true;
        }
        else
        {
            m_store.commit([this](domain::v1::Workspace& ws) { m_after.get_to(ws); });
        }
    }

    void Undo() override
    {
        m_store.commit([this](domain::v1::Workspace& ws) { m_before.get_to(ws); });
    }

private:
    domain::v1::WorkspaceStore& m_store;
    std::string m_name;
    std::function<void()> m_action;
    nlohmann::json m_before;
    nlohmann::json m_after;
    bool m_hasRun = false;
};

class TestCommand final : public ICommand {
public:
    TestCommand(domain::v1::Workspace* workspace)
    {
    }

    const char* Name() const override { return "Test"; }

    void Do() override
    {
    }

    void Undo() override
    {
    }

};



class IWorkspaceService
{

public:
    virtual  ~IWorkspaceService() = default;

    virtual  bool loadWorkspace(std::string path, std::string fileName) = 0;
    virtual  void saveWorkspace(std::string path, std::string fileName) = 0;


};

class WorkspaceService final : public IWorkspaceService {
private:
    domain::DataContext& m_dataContext;
    ModelCache& m_cache;
    TaskRunner* m_taskRunner;
    domain::v1::WorkspaceStore* m_workspaceStore;
public:
    WorkspaceService(domain::DataContext& dataContext, domain::v1::WorkspaceStore& workspaceStore, ModelCache& cache, TaskRunner* taskRunner);
    ~WorkspaceService();
    bool loadWorkspace(std::string path, std::string fileName) override;
    void saveWorkspace(std::string path, std::string fileName) override;



};

WorkspaceService::WorkspaceService(domain::DataContext& dataContext, domain::v1::WorkspaceStore& workspaceStore, ModelCache& cache, TaskRunner* taskRunner)
    : m_dataContext(dataContext), m_cache(cache)
{
    m_workspaceStore = &workspaceStore;
}
WorkspaceService::~WorkspaceService()
{
}

bool WorkspaceService::loadWorkspace(std::string path, std::string fileName) {
    std::ifstream file(path + "\\" + fileName);

    if (!file) return false;

    std::string tok;
    bool loadedOk = true;

    m_workspaceStore->commit([&](domain::v1::Workspace& ws)
        {
            nlohmann::json j;
            file >> j;

            try {
                j.get_to(ws);

            }
            catch (const nlohmann::json::exception& e) {
                printf("[WorkspaceService] loadWorkspace: failed to parse %s: %s\n",
                    (path + "\\" + fileName).c_str(), e.what());
                loadedOk = false;
                return;
            }
            loadedOk = true;
            FolderScanner scanner = FolderScanner();

            for each(domain::v1::Project * project in ws.projects)
            {
                for each(domain::v1::BuildPlate * buildPlate in project->buildPlates)
                {


                    std::string path = "C:\\github\\Pistachio-config\\Assets\\STL\\BuildPlate.stl";

                    StlLoaderAdapter loader = StlLoaderAdapter(m_cache);
                    loader.load(path);

                    std::shared_ptr<domain::v1::Model> buildPlateModel = std::make_shared<domain::v1::Model>();

                    buildPlateModel->mesh = loader.getMesh();
                    buildPlateModel->Id = scanner.scanFile(path).file.fileHash.c_str();

                    buildPlate->buildPlateModel = buildPlateModel;

                    ModelInstanceFactory factory;

                    for each(auto& instance in buildPlate->modelInstances)
                    {
                        auto asset = m_cache.assets.find(instance->modelHash);

                        StlLoaderAdapter loader = StlLoaderAdapter(m_cache);
                        loader.load(asset->second->fileLocation);


                        std::shared_ptr<domain::v1::Model> m = std::make_shared<domain::v1::Model>();
                        m->mesh = loader.getMesh();
                        m->Id = instance->modelHash;
                        m->fileName = asset->second->fileName;
                        m->fileLocation = asset->second->fileLocation;
                        m->label = path.c_str();
                        m->name = asset->second->name;
                        m_cache.models[instance->modelHash] = m;

                    }



                }
            }
        });


    return loadedOk;

}
void WorkspaceService::saveWorkspace(std::string path, std::string fileName) {

}

class ISlicerService
{

public:
    virtual  ~ISlicerService() = default;



};

class SlicerService final : public ISlicerService {

private:
    domain::DataContext& m_dataContext;
    ModelCache& m_cache;
    TaskRunner* m_taskRunner;
    domain::v1::WorkspaceStore* m_workspaceStore;

public:
    SlicerService(domain::DataContext& dataContext, domain::v1::WorkspaceStore& workspaceStore, ModelCache& cache, TaskRunner* taskRunner);
    ~SlicerService();

    void loadWorkspace();
    void addModel(domain::v1::BuildPlate* selectedBuildPlate, const void* data, size_t size);
    void arrangeBuildPlate(domain::v1::BuildPlate* selectedBuildPlate);

    bool saveWorkspaceToFile(const std::string& filePath);

};

SlicerService::SlicerService(domain::DataContext& dataContext, domain::v1::WorkspaceStore& workspaceStore, ModelCache& cache, TaskRunner* taskRunner)
    : m_dataContext(dataContext), m_cache(cache)
{
    m_workspaceStore = &workspaceStore;

}
SlicerService::~SlicerService() {

}

// Helper to convert millimeter floats to Clipper's integer scaling system (6 decimals precision)
const float SCALE_FACTOR = 1000000.0f;
inline Point64 ToPoint64(float x, float z) {
    return Point64(static_cast<int64_t>(x * SCALE_FACTOR), static_cast<int64_t>(z * SCALE_FACTOR));
}
inline glm::vec2 FromPoint64(const Point64& pt) {
    return glm::vec2(static_cast<float>(pt.x) / SCALE_FACTOR, static_cast<float>(pt.y) / SCALE_FACTOR);
}

Paths64 GetModel2DPath(domain::v1::Model* model, float rotationY) {
    // 0,0,0 is the center. Width is X, Depth is Y.
    float halfW = (model->mesh->bounds.max.x - model->mesh->bounds.min.x) * 0.5f;
    float halfD = (model->mesh->bounds.max.y - model->mesh->bounds.min.y) * 0.5f; // Depth on Y

    // Define 4 corners relative to the model's central origin (0,0)
    glm::vec2 corners[4] = {
        {-halfW, -halfD}, {halfW, -halfD}, {halfW, halfD}, {-halfW, halfD}
    };

    float rad = glm::radians(rotationY);
    float cosR = std::cos(rad);
    float sinR = std::sin(rad);

    Path64 path;
    for (int i = 0; i < 4; ++i) {
        float rotX = corners[i].x * cosR - corners[i].y * sinR;
        float rotY = corners[i].x * sinR + corners[i].y * cosR; // Rotating X and Y
        path.push_back(ToPoint64(rotX, rotY));
    }

    Paths64 result;
    result.push_back(path);
    return result;
}

void SlicerService::arrangeBuildPlate(domain::v1::BuildPlate* selectedBuildPlate) {

    auto startTime = std::chrono::high_resolution_clock::now();

    if (!selectedBuildPlate || !selectedBuildPlate->buildPlateModel) {
        std::cout << "[ARRANGE] Error: Selected BuildPlate or its bounds are null.\n";
        return;
    }

    auto bpBounds = selectedBuildPlate->buildPlateModel->mesh->bounds;

    float halfPlateW = (bpBounds.max.x - bpBounds.min.x) * 0.5f;
    float halfPlateD = (bpBounds.max.y - bpBounds.min.y) * 0.5f;

    // bpCenter is the center of the *logical* placement space instances
    // live in -- front-left corner = local (0,0), back-right corner =
    // local (bedSize, bedSize) -- not the plate mesh's own local/CAD
    // coordinates (bpBounds above, which is a separate, unrelated frame).
    // For a 350mm bed that logical space's center is (175, 175), i.e.
    // exactly (halfPlateW, halfPlateD): the bed's half-size, not (0,0).
    glm::vec2 bpCenter(halfPlateW, halfPlateD);

    // Sort instances by resolved mesh footprint area, largest first
    std::sort(selectedBuildPlate->modelInstances.begin(), selectedBuildPlate->modelInstances.end(),
        [this](const std::unique_ptr<ModelInstance>& a, const std::unique_ptr<ModelInstance>& b) {
            auto modelA = m_cache.getModel(a->modelHash);
            auto modelB = m_cache.getModel(b->modelHash);
            if (!modelA || !modelB || !modelA->mesh || !modelB->mesh) return false;
            float areaA = (modelA->mesh->bounds.max.x - modelA->mesh->bounds.min.x) * (modelA->mesh->bounds.max.y - modelA->mesh->bounds.min.y);
            float areaB = (modelB->mesh->bounds.max.x - modelB->mesh->bounds.min.x) * (modelB->mesh->bounds.max.y - modelB->mesh->bounds.min.y);
            return areaA > areaB;
        });

    struct PlacedRect { float cx, cy, halfW, halfD; };
    std::vector<PlacedRect> placedRects;
    placedRects.reserve(selectedBuildPlate->modelInstances.size());

    float padding = 10.0f; // 2mm safety gap
    int placedCount = 0;
    int failedCount = 0;
    long long totalPositionsTested = 0;

    for (auto& instance : selectedBuildPlate->modelInstances) {

        auto model = m_cache.getModel(instance->modelHash);
        if (!model || !model->mesh) {
            std::cout << "[ARRANGE] Skipped \"" << instance->name << "\": no local bounds.\n";
            continue;
        }

        auto modelStart = std::chrono::high_resolution_clock::now();

        float halfW = (model->mesh->bounds.max.x - model->mesh->bounds.min.x) * 0.5f;
        float halfD = (model->mesh->bounds.max.y - model->mesh->bounds.min.y) * 0.5f;
        float maxRadius = std::sqrt(halfPlateW * halfPlateW + halfPlateD * halfPlateD);

        // The packing search below (testX +/- halfW etc.) works in terms of
        // the part's FOOTPRINT CENTER -- but StandardModelRenderStrategy::
        // computeModelMatrix() places transform.position at the mesh's own
        // LOCAL ORIGIN point, not its footprint center. Those only coincide
        // if a mesh happens to be modeled with its local origin exactly at
        // its own center; our test cube's local origin is at a *corner*
        // (bounds 0..30), so every part placed here has been landing
        // offset from its intended footprint position by this amount --
        // small enough on a 30-unit cube against a 350mm bed to go
        // unnoticed, obvious on something larger like a skirt. This
        // converts a footprint-center placement into the correct
        // local-origin placement before it's written to transform.position
        // below, so the two conventions stay consistent regardless of
        // where a given mesh's own local origin happens to sit.
        glm::vec2 localCenterOffset(
            (model->mesh->bounds.min.x + model->mesh->bounds.max.x) * 0.5f,
            (model->mesh->bounds.min.y + model->mesh->bounds.max.y) * 0.5f);

        bool placed = false;
        glm::vec2 bestPos(0.0f);
        long long positionsTested = 0;

        for (float r = 0; r < maxRadius; r += 2.0f) {
            int steps = (r == 0.0f) ? 1 : static_cast<int>(2.0f * M_PI * r / 4.0f);
            bool foundAtThisRadius = false;

            for (int i = 0; i < steps; ++i) {
                float angle = (r == 0.0f) ? 0.0f : (2.0f * M_PI * i / steps);
                float testX = bpCenter.x + r * std::cos(angle);
                float testY = bpCenter.y + r * std::sin(angle);
                positionsTested++;

                // Boundary check is relative to bpCenter, not a hardcoded
                // (0,0)-centered plate -- otherwise this still bounds
                // candidates against the *old* search origin even though
                // bpCenter has moved, which is what pulled placement off
                // to one side instead of landing exactly on bpCenter for
                // a lone part.
                if ((testX - halfW) < (bpCenter.x - halfPlateW) || (testX + halfW) > (bpCenter.x + halfPlateW) ||
                    (testY - halfD) < (bpCenter.y - halfPlateD) || (testY + halfD) > (bpCenter.y + halfPlateD)) {
                    continue;
                }

                bool overlaps = false;
                for (const auto& rect : placedRects) {
                    if (std::abs(testX - rect.cx) < (halfW + rect.halfW + padding) &&
                        std::abs(testY - rect.cy) < (halfD + rect.halfD + padding)) {
                        overlaps = true;
                        break;
                    }
                }

                if (!overlaps) {
                    bestPos = glm::vec2(testX, testY);
                    placed = true;
                    foundAtThisRadius = true;
                    break;
                }
            }
            if (foundAtThisRadius) break;
        }

        auto modelEnd = std::chrono::high_resolution_clock::now();
        double modelMs = std::chrono::duration<double, std::milli>(modelEnd - modelStart).count();
        totalPositionsTested += positionsTested;

        if (placed) {
            // bestPos is the footprint CENTER (see localCenterOffset
            // comment above); subtracting the mesh's own local-origin-to-
            // center offset here converts it to the correct local-origin
            // placement the renderer actually expects.
            instance->transform.position.x = bestPos.x - localCenterOffset.x;
            instance->transform.position.y = bestPos.y - localCenterOffset.y;
            instance->transform.position.z = 0.0f;
            placedRects.push_back({ bestPos.x, bestPos.y, halfW, halfD });
            placedCount++;

            std::cout << "[ARRANGE] Placed \"" << instance->name << "\" at (" << bestPos.x << ", " << bestPos.y << ")"
                << " | positionsTested=" << positionsTested << " | time=" << modelMs << " ms\n";
        }
        else {
            failedCount++;
            // Past the back-right corner of the logical placement space
            // (bpCenter + half-size), not bpBounds.max -- that's the
            // plate mesh's own local/CAD coordinates, a different frame.
            // Same local-origin-vs-footprint-center conversion as the
            // placed branch above.
            instance->transform.position.x = ((bpCenter.x + halfPlateW) + 20) - localCenterOffset.x;
            instance->transform.position.y = ((bpCenter.y + halfPlateD) + 20) - localCenterOffset.y;
            instance->transform.position.z = 0.0f;

            std::cout << "[ARRANGE] FAILED to place \"" << instance->name << "\""
                << " | positionsTested=" << positionsTested << " | time=" << modelMs << " ms\n";
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    std::cout << "[ARRANGE] Done in " << totalMs << " ms | placed=" << placedCount << " failed=" << failedCount
        << " | totalPositionsTested=" << totalPositionsTested << "\n";
}
bool SlicerService::saveWorkspaceToFile(const std::string& filePath) {
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for writing: " << filePath << std::endl;
            return false;
        }

        // Implicitly converts context to json, then pretty-prints with 4-space indentation
        m_workspaceStore->read([&](const domain::v1::Workspace& ws)
            {
                nlohmann::json j = ws;
                file << j.dump(4);
            });

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during save: " << e.what() << std::endl;
        return false;
    }
}


void SlicerService::addModel(domain::v1::BuildPlate* selectedBuildPlate, const void* data, size_t size) {


    /*m_taskRunner->group("Import Folders")
        .sequential()
        .stopOnFailure(true)
        .showSubSteps(true)
        .step("Scanning files..", [this, selectedBuildPlate, data, size](auto prog )
        {*/
    std::string raw((const char*)data, size);
    std::istringstream pathString(raw);
    std::string path;
    FolderScanner folderScanner;
    while (std::getline(pathString, path))
    {
        if (!path.empty())
        {
            printf("[SlicerCore] dropped: %s\n", path.c_str());

            // load mesh and add to build plate
            StlLoaderAdapter loader = StlLoaderAdapter(m_cache);
            loader.load(path);


            ScanFileResult result = folderScanner.scanFile(path);
            if (!result.success) {
                // errored
                return;
            }

            std::shared_ptr<domain::v1::Model> m = std::make_shared<domain::v1::Model>();
            m->mesh = loader.getMesh();
            m->Id = result.file.fileHash;
            m->fileName = result.file.filename;
            m->fileLocation = result.file.fullPath;
            m->name = result.file.name;

            m->label = path.c_str();



            m_cache.models[result.file.fileHash] = m;

            auto it = this->m_cache.assets.find(result.file.fileHash);
            if (it == m_cache.assets.end() || !it->second)
            {
                return;
            }

            domain::v1::ImportedAsset& asset = *it->second;
            ModelInstanceFactory factory;

            Mesh* mesh = loader.getMesh().get();

            std::vector<std::unique_ptr<ModelInstance>>  modelInstances = factory.createFromFileMeta(asset, mesh);

            for (auto& instance : modelInstances)
            {

                selectedBuildPlate->modelInstances.push_back(std::move(instance));
            }



        }
    }
    //})
    //.completed([this](bool ok) {
    //    //m_buildPlateRenderer.updateViewModel(m_selectedProject->buildPlates);

    //    })
    //.submit();
}



void SlicerService::loadWorkspace() {



    domain::v1::Project* project = new domain::v1::Project();

    project->Id = utils::generateGuid();

    project->name = "Test Project";
    project->label = "Test Project";

    std::string path = "C:\\github\\Pistachio-config\\Assets\\STL\\BuildPlate.stl";


    StlLoaderAdapter loader = StlLoaderAdapter(m_cache);
    loader.load(path);

    FolderScanner scanner = FolderScanner();

    domain::v1::BuildPlate* buildPlate = new domain::v1::BuildPlate();
    buildPlate->Id = utils::generateGuid();
    buildPlate->name = "Test build plate";
    buildPlate->label = "created from loadWorkspaces";


    std::shared_ptr<domain::v1::Model> buildPlateModel = buildPlate->buildPlateModel;
    buildPlateModel->mesh = loader.getMesh();
    buildPlateModel->Id = scanner.scanFile(path).file.fileHash.c_str();

    buildPlate->buildPlateModel = buildPlateModel;
    project->buildPlates.push_back(buildPlate);


    //m_dataContext.m_workspace->projects.push_back(project);
    m_workspaceStore->commit([&](domain::v1::Workspace& ws) {
        ws.projects.push_back(project);
        });

}

// -----------------------------------------------------------------------
// SlicerCorePlugin
// -----------------------------------------------------------------------
class SlicerCorePlugin final : public IUiModule
{
public:
    //SlicerCorePlugin(ModelCache& modelCache);
     // ~SlicerCorePlugin() override = default;


     // -----------------------------------------------------------------------
    void onLoad(UiHostServices& svc, domain::DataContext& dataContext) override
    {
        printf("[SlicerCore] onLoad\n");

        m_modelCache = svc.application->services().resolve<domain::v1::ModelCache>();
        m_workspaceStore = svc.application->services().resolve<domain::v1::WorkspaceStore>();

        m_taskreporter = svc.application->services().resolve<ports::ITaskProgressReporter>();

        //m_workspace = dataContext.m_workspace;
        /* refactor the load adapters to use the cache and OCCT from the plug-in
        adapters::StepFileLoader step;
        std::shared_ptr<domain::Model> mode = step.load("C:\\github\\StepFileExtraction\\Debug\\Node_1\\Node_1_3\\Node_1_3_1\\Node_1_3_1_1.step");
        m_app->loadFile("C:\\github\\StepFileExtraction\\Debug\\Node_1\\Node_1_3\\Node_1_3_1\\Node_1_3_1_1.step");
        */

        // FolderScanner scanner1;
        //ScanFileResult hash = scanner1.scanFile ("E:\\github\\Voron-2\\STLs\\Test_Prints\\Voron_Design_Cube_v7.stl");


        m_app = reinterpret_cast<core::Application*>(svc.app);
        m_config = reinterpret_cast<ports::IConfigPort*>(svc.config);
        m_registry = reinterpret_cast<adapters::ContributionRegistry*>(svc.registry);
        m_guiHost = reinterpret_cast<adapters::ImGuiHost*>(svc.guiHost);
        //taskRunner = reinterpret_cast<TaskRunner*>(svc.taskRunner);
        taskRunner = svc.application->services().resolve<TaskRunner>();

        m_workspaceService = new WorkspaceService(dataContext, *m_workspaceStore, *m_modelCache, taskRunner);
        m_slicerService = new SlicerService(dataContext, *m_workspaceStore, *m_modelCache, taskRunner);




        m_slicerService->loadWorkspace();
        m_eventBus = svc.application->services().resolve<ports::IEventBus>();

        // SlicerCorePlugin::onLoad(), alongside the existing initialize() call
        m_viewportRenderRegistry = svc.application->services().resolve<ports::IViewportRendererRegistry>();
        if (m_viewportRenderRegistry)
        {
            m_viewportController = std::make_unique<ViewportController>(*m_viewportRenderRegistry);

            m_eventBus->subscribe("viewcontroler.setactive.toolpath_ribbon", [this](const std::string& payload) {

                m_viewportController->setActiveRenderer("toolpath_ribbon");

                });

        }




        taskRunner->group("Startup Load")
            .sequential()
            .stopOnFailure(true)
            .step("Scanning asset library", [this](std::shared_ptr<TaskProgress> progress)
                {
                    progress->setMessage("Scanning  Voron-2 STLs");

                    auto tempproject = std::make_shared<domain::v1::Project>();

                    CachedFolderScanner scanner;   // instead of FolderScanner
                    ScanOptions opts;
                    opts.recursive = true;
                    opts.includeHidden = false;
                    opts.maxDepth = 10;

                    ScanResult result = scanner.scan("e:\\github\\Voron-2\\STLs", opts);
                    // optional 3rd/4th args: scanner.scan(path, opts, ".cache", /*fresh=*/false)

                    if (!result.success)
                    {
                        printf("[SlicerCore] scan failed: %s\n", result.errorMessage.c_str());
                        progress->failed = true;
                        return;
                    }
                    printf("Scanned: %s — %d files, %d folders\n", result.rootPath.c_str(), result.totalFiles, result.totalFolders);
                    tempproject->fromScanResult(result, *m_modelCache);
                    m_scannedProject = tempproject;
                    m_projectLoaded = true;


                })
            /* .step("Loading workspace", [this](std::shared_ptr<TaskProgress> progress)
                 {

                     progress->setMessage("Loading workspace.json");
                     m_workspaceService->loadWorkspace("c:\\temp\\", "workspace.json");
                 })*/
            .completed([this](bool success)
                {
                    printf("[SlicerCore] startup load %s\n", success ? "complete" : "FAILED");



                    if (success)
                    {

                        m_editableScene = std::make_unique<EditableSceneGLRender>();
                        m_viewportRenderRegistry->registerRenderer("editable_scene", m_editableScene.get());
                        m_viewportController->setActiveRenderer("editable_scene");

                        m_multiPlateScene = std::make_unique<MultiPlateSceneGLRender>();
                        m_viewportRenderRegistry->registerRenderer("multi_plate_scene", m_multiPlateScene.get());

                        //m_debugComparison = std::make_unique<DebugComparisonGLRender>();
                        //m_viewportRenderRegistry->registerRenderer("debug_comparison", m_debugComparison.get());


                        //if (modelCache) m_debugComparison->loadModel(*modelCache, "f10d5414c209e764");
                        //if (modelCache) m_debugComparison->loadModel(*modelCache, "fd0d5d3656e5b8a7");

                        // m_viewportController->setTarget(glm::vec3(177.0f, 177.0f, 0.4f));

                         // now update the UI.
                        auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                        auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);

                        m_slicerService->arrangeBuildPlate(buildPlate);

                        //m_buildPlateRenderer->updateViewModel(project->buildPlates);

                        m_editableScene->sceneLayout().setActiveBuildPlate(buildPlate, *m_modelCache);
                        m_multiPlateScene->sceneLayout().createLayout(project->buildPlates, *m_modelCache);

                    }
                })
            .submit();

        // need a better way to set these.
        m_navigation = new NavigationManager();
        //m_navigation->setProject(m_workspace->projects[0]->Id);
        //m_navigation->setBuildPlate(m_workspace->projects[0]->buildPlates[0]->Id);

        //auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
        //auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);




        m_preview = slicer::StlPreviewRenderer();

        m_cmdHistory.OnHistoryChanged = [this]()
            {
                auto* project = m_navigation->resolveProject(*m_workspaceStore);
                auto* buildPlate = m_navigation->resolveBuildPlate(project);

                //if (!buildPlate)
                //    m_navigation->setBuildPlate("");

                //m_buildPlateRenderer->updateViewModel(project ? project->buildPlates : std::vector<domain::v1::BuildPlate*>{});
            };

        //if (buildPlate)
        //    m_slicerService->arrangeBuildPlate(buildPlate);

        //m_buildPlateRenderer = std::make_unique<slicer::BuildPlateRenderer>();

        //printf("[SlicerCore] about to call initialize, project=%p, buildPlates.size()=%zu\n",
        //    (void*)m_selectedProject, m_selectedProject ? m_selectedProject->buildPlates.size() : 0);

        //m_buildPlateRenderer->initialize(*m_workspaceStore, project, *m_modelCache, *m_navigation);
       // printf("[SlicerCore] initialize() returned\n");

        if (m_registry)
        {



            // add menu items
            m_menuContrib = m_registry->contributeMenu(k_pluginId, "Slicer", 300);
            m_menuContrib->addItem(
                "slice_now", "Slice Now", 10,
                [this]() { m_sliceRequested = true; },
                "Ctrl+Shift+S");
            m_menuContrib->addSeparator(90);
            //m_menuContrib->addToggle(
            //    "slicer_panel_open", "Show Slicer Panel", 100, &m_panelOpen);

            // add ribbon items
            m_ribbonContrib = m_registry->contributeRibbon(k_pluginId, "Slicer", 300);


            m_ribbonContrib->addButton("toggle_viewport", "Toggle View", "", 60, [this]() {
                // Single/multi build-plate toggle -- animated via
                // ViewportController::animateTo() so the camera eases
                // between views instead of cutting, and via
                // MultiPlateSceneLayout::animateGhostOutExcept()/
                // animateGhostAllTo() so the multi-plate scene itself
                // fades out/in rather than popping (see
                // beginSwitchToEditable()/beginSwitchToMultiPlate() above).
                if (!m_viewportController || !m_editableScene || !m_multiPlateScene) return;

                if (m_viewportController->activeId() == "multi_plate_scene")
                    beginSwitchToEditable(nullptr);
                else
                    beginSwitchToMultiPlate();
                });

            // Diagnostic only -- see the long comment on CameraState::
            // axisMode in ports/I3DViewportGLRender.h. Cycles the camera
            // basis through a few candidate up/right conventions -- affects
            // every registered renderer at once (editable scene, debug
            // visualiser), since they all share one CameraState instance
            // via m_viewportController. Mode 0 ("Default") is the
            // confirmed-correct convention already baked in, so this is
            // purely for exploring/confirming further, not a hidden
            // regression risk.
            /*m_ribbonContrib->addCustom("cam_axis_mode", 62, [this]() {
                if (!m_viewportController) return;

                char label[64];
                snprintf(label, sizeof(label), "Cam Axis: %s", m_viewportController->cameraAxisModeName());
                if (ImGui::Button(label))
                {
                    m_viewportController->cycleCameraAxisMode();
                }
                });*/

            m_ribbonContrib->addButton("slice_now", "Slice", ICON_FA_CUBES, 10, [this]() {

                // now update the UI.
                auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);

                printf("[SlicerCore] publishing run.pipeline for plate %s\n", buildPlate->Id.c_str());
                m_eventBus->publish("run.pipeline", buildPlate->Id);

                });
            /*m_ribbonContrib->addToggle("slicer_panel", "Panel", 90, &m_panelOpen);*/
            m_ribbonContrib->addSeparator(20);
            /* m_ribbonContrib->addButton("refresh_view", "Refresh", "", 20, [this, project]() {
                 this->m_buildPlateRenderer->updateViewModel(project->buildPlates);  });*/

            m_ribbonContrib->addButton("arrange_build_plate", "Arrange", ICON_FA_TH, 20, [this]() {

                auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);

                m_slicerService->arrangeBuildPlate(buildPlate);
                m_editableScene->sceneLayout().setActiveBuildPlate(buildPlate, *m_modelCache);
                m_multiPlateScene->sceneLayout().createLayout(project->buildPlates, *m_modelCache);
                });
            m_ribbonContrib->addSeparator(30);

            // Load Workspace
           /* m_ribbonContrib->addButton("load_workspace", "Load", ICON_FA_FOLDER_OPEN, 30, [this]() {
                auto cmd = std::make_unique<SnapshotCommand>(
                    *m_workspaceStore, "Load Worspace",
                    [this]() {

                        m_workspaceService->loadWorkspace("c:\\temp\\", "workspace.json");

                        auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                        auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);

                        m_slicerService->arrangeBuildPlate(buildPlate);
                        m_editableScene->sceneLayout().setActiveBuildPlate(buildPlate, *m_modelCache);

                    });
                m_cmdHistory.Execute(std::move(cmd));

                });*/

            m_ribbonContrib->addButton("load_workspace", "Load", ICON_FA_FOLDER_OPEN, 30, [this]() {

                HWND ownerHwnd = m_guiHost ? glfwGetWin32Window(m_guiHost->window()) : nullptr;

                auto selectedPath = core::showOpenFileDialog(
                    L"Load Workspace",
                    { { L"Workspace Files (*.json)", L"*.json" } },
                    ownerHwnd
                );
                if (!selectedPath.has_value()) {
                    return; // user cancelled the dialog
                }

                std::filesystem::path filePath(*selectedPath);

                auto cmd = std::make_unique<SnapshotCommand>(
                    *m_workspaceStore, "Load Workspace",
                    [this, filePath]() {
                        m_workspaceService->loadWorkspace(
                            filePath.parent_path().string(),
                            filePath.filename().string());
                        auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                        auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);
                        m_slicerService->arrangeBuildPlate(buildPlate);
                        m_editableScene->sceneLayout().setActiveBuildPlate(buildPlate, *m_modelCache);
                        m_multiPlateScene->sceneLayout().createLayout(project->buildPlates, *m_modelCache);
                    });
                m_cmdHistory.Execute(std::move(cmd));
                });

            m_ribbonContrib->addButton("saveWorkspace", "Save", ICON_FA_FILE, 30, [this]() {
                auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);

                const std::string fileName = "c:\\temp\\workspace.json";
                m_slicerService->saveWorkspaceToFile(fileName);
                m_buildPlateRenderer->updateViewModel(project->buildPlates);
                });
            m_ribbonContrib->addSeparator(40);

            // undo / redo
            auto undoRedoContrib = m_registry->contributeRibbon(k_pluginId, "UndoRedo", 400);
            undoRedoContrib->addButton("undo", "Undo", ICON_FA_UNDO, 40, [this]() { m_cmdHistory.Undo(); });
            undoRedoContrib->addButton("redo", "Redo", ICON_FA_REDO, 40, [this]() { m_cmdHistory.Redo(); });

            // Single / Multi
           /* auto sceneLayoutContrib = m_registry->contributeRibbon(k_pluginId, "UndoRedo", 400);
            sceneLayoutContrib->addButton("changeToSingle", "Single", "", 40, [this]() {  m_buildPlateRenderer->getSceneLayout().selectPlate(0); });
            sceneLayoutContrib->addButton("changeToMulti", "Multi", "", 40, [this]() { m_buildPlateRenderer->getSceneLayout().selectPlate(-1); });*/


            auto partContrib = m_registry->contributeRibbon(k_pluginId, "Part", 400);
            // delete Part
            /*partContrib->addButton("deletePart", "Delete", "", 40, [this]() {
                auto selectedIds = m_navigation->selection().getSelectedIds();
                if (selectedIds.empty()) return;

                auto cmd = std::make_unique<SnapshotCommand>(
                    *m_workspaceStore, "Delete Instances",
                    [this, selectedIds]() {
                        auto* project = m_navigation->resolveProject(*m_workspaceStore);
                        if (!project) return;
                        auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);
                        if (!buildPlate) return;

                        m_workspaceStore->commit([&](domain::v1::Workspace&) {
                            auto& instances = buildPlate->modelInstances;
                            instances.erase(
                                std::remove_if(instances.begin(), instances.end(),
                                    [&](const std::unique_ptr<ModelInstance>& inst) {
                                        return selectedIds.count(inst->id) > 0;
                                    }),
                                instances.end());
                            });

                        m_navigation->selection().clear();
                        m_buildPlateRenderer->updateViewModel(project->buildPlates);
                    });
                m_cmdHistory.Execute(std::move(cmd));
                });*/


                //auto plateContrib = m_registry->contributeRibbon(k_pluginId, "Plate", 400);
                //plateContrib->addButton("addPlate", "Add", "", 60, [this]() {

                //    // Add plate
                //    auto addCmd = std::make_unique<SnapshotCommand>(
                //        *m_workspaceStore, "Add Build Plate",
                //        [this]() {
                //            auto* project = m_navigation->resolveProject(*m_workspaceStore);
                //            if (!project) return;

                //            auto* plate = new domain::v1::BuildPlate();
                //            plate->Id = utils::generateGuid();
                //            plate->name = "Build Plate " + std::to_string(project->buildPlates.size() + 1);
                //            // TODO: populate plate->buildPlateModel same way SlicerService::loadWorkspace() does
                //            std::shared_ptr<domain::v1::Model> buildPlateModel = plate->buildPlateModel;
                //            FolderScanner scanner;

                //            std::string path = "C:\\github\\Pistachio-config\\Assets\\STL\\BuildPlate.stl";

                //            StlLoaderAdapter loader = StlLoaderAdapter(*m_modelCache);
                //            loader.load(path);

                //            buildPlateModel->mesh = loader.getMesh();

                //            buildPlateModel->Id = scanner.scanFile(path).file.fileHash.c_str();

                //            plate->buildPlateModel = buildPlateModel;
                //            project->buildPlates.push_back(plate);
                //            m_navigation->setProject(project->Id);   // keep context consistent
                //            m_navigation->setBuildPlate(plate->Id);  // select the new plate immediately
                //        });
                //    m_cmdHistory.Execute(std::move(addCmd));


                //    });


                // add dragdrop target
            m_dragDropContrib = m_registry->contributeDragDrop(
                k_pluginId, "ASSET_PATHS", 100);

            m_dragDropContrib->onDrop = [this](const void* data, size_t size)
                {

                    auto cmd = std::make_unique<SnapshotCommand>(
                        *m_workspaceStore, "Drop Models",
                        [this, data, size]() {

                            auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                            auto* buildPlate = m_navigation->resolveOrDefaultBuildPlate(project);

                            m_slicerService->addModel(buildPlate, data, size);
                            m_slicerService->arrangeBuildPlate(buildPlate);

                            const std::string fileName = "c:\\temp\\workspace.json";
                            m_slicerService->saveWorkspaceToFile(fileName);
                            m_buildPlateRenderer->updateViewModel(project->buildPlates);

                        });
                    m_cmdHistory.Execute(std::move(cmd));
                };

            m_dragDropContrib->onHoverRender = [this]()
                {
                    // Optional — draw a highlight overlay while dragging over
                   /* ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetForegroundDrawList()->AddRect(
                        min, max, IM_COL32(100, 200, 255, 200), 4.0f, 0, 2.0f);*/
                };
        }

        m_viewportController->setActiveRenderer("editable_scene");

        printf("[SlicerCore] onLoad done\n");
    }

    // -----------------------------------------------------------------------
    void onUnload(UiHostServices& svc, domain::DataContext& dataContext) override
    {
        printf("[SlicerCore] onUnload\n");
        auto* registry = reinterpret_cast<adapters::ContributionRegistry*>(svc.registry);
        if (registry)
            registry->removeAllContributions(k_pluginId);


        m_dragDropContrib = nullptr;
        m_menuContrib = nullptr;
        m_ribbonContrib = nullptr;
        m_registry = nullptr;
        m_app = nullptr;
        m_config = nullptr;
        m_selectedAssetId = -1;
        m_preview.clear();


    }

    // -----------------------------------------------------------------------
    void render(UiHostServices&, domain::DataContext& dataContext) override
    {
        auto renderStart = std::chrono::high_resolution_clock::now();

        if (m_sliceRequested)
        {
            // TODO: trigger slice
            m_sliceRequested = false;
        }

        if (m_guiHost->m_initialized && m_filesLoaded == false) {

            m_filesLoaded = true;
            //taskRunner->group("Import Folders")
            //    .sequential()
            //    .stopOnFailure(true)
            //    .showSubSteps(true)
            //    .step("Scanning files..",[this](auto prog) {

            //            prog->setMessage("Waiting...");
            //            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            //            // Scan and build project
            //            FolderScanner  scanner;
            //            ScanOptions    opts;
            //            opts.recursive = true;
            //            opts.includeHidden = false;
            //            opts.maxDepth = 10;

            //            prog->setMessage("Scanning...");

            //            opts.onProgress = [](const std::string& path, int found) -> bool {
            //                printf("Scanning: %s  (%d found)\n", path.c_str(), found);
            //                return true;
            //                };

            //            

            //        })
            //     
            //    .submit();

        }
        //taskRunner-> renderUI();

        if (m_taskreporter) m_taskreporter->display();

        auto afterTaskRunner = std::chrono::high_resolution_clock::now();

        //renderSlicerPanel();
        auto afterSlicer = std::chrono::high_resolution_clock::now();

        renderBuildPlatePanel();

        auto renderEnd = std::chrono::high_resolution_clock::now();
        static int fc = 0;
        if (++fc % 30 == 0)
        {
            auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
            printf("[perf] TOTAL=%.2fms | taskRunnerUI=%.2fms | slicerPanel=%.2fms | buildPlatePanel=%.2fms\n",
                ms(renderStart, renderEnd), ms(renderStart, afterTaskRunner), ms(afterTaskRunner, afterSlicer), ms(afterSlicer, renderEnd));
        }

    }

private:
    static constexpr const char* k_pluginId = "pistachio.slicer_core";

    core::commands::CommandHistory m_cmdHistory;

    SlicerService* m_slicerService = nullptr;
    WorkspaceService* m_workspaceService = nullptr;
    std::unique_ptr<ViewportController> m_viewportController;
    ports::IViewportRendererRegistry* m_viewportRenderRegistry;


    ports::IEventBus* m_eventBus = nullptr;


    core::Application* m_app = nullptr;
    ports::IConfigPort* m_config = nullptr;
    adapters::ContributionRegistry* m_registry = nullptr;
    adapters::ImGuiHost* m_guiHost = nullptr;
    ports::MenuContribution* m_menuContrib = nullptr;
    ports::RibbonContribution* m_ribbonContrib = nullptr;
    ports::DragDropContribution* m_dragDropContrib = nullptr;

    //std::unique_ptr<domain::v1::Project> m_project;


    domain::v1::Workspace* m_workspace;
    domain::v1::WorkspaceStore* m_workspaceStore = nullptr;

    NavigationManager* m_navigation;

    TaskRunner* taskRunner;
    ModelCache* m_modelCache; // the 3d mesh of the stls that have been loaded
    ports::ITaskProgressReporter* m_taskreporter = nullptr;

    int   m_selectedAssetId = -1;

    std::unordered_set<std::string> s_selected;

    int   m_selectedPlateId = -1;
    bool  m_panelOpen = true;
    bool  m_sliceRequested = false;

    bool m_filesLoaded = false;
    bool m_projectLoaded = false;
    std::shared_ptr<domain::v1::Project> m_scannedProject;

    float m_treeHeight = 800.0f;
    float m_previewHeight = 180.0f;
    slicer::StlPreviewRenderer m_preview;

    std::unique_ptr<slicer::BuildPlateRenderer> m_buildPlateRenderer;

    std::unique_ptr<EditableSceneGLRender> m_editableScene;
    std::unique_ptr<MultiPlateSceneGLRender> m_multiPlateScene;

    // True while multi_plate_scene is fading itself out ahead of an
    // impending swap to editable_scene -- see beginSwitchToEditable() and
    // the check at the top of renderBuildPlate(). multi_plate_scene must
    // stay the active renderer for the ghost fade to actually be visible
    // (it's the one drawing itself translucent), so the renderer swap is
    // deferred until the fade completes rather than happening immediately.
    bool m_pendingSwitchToEditable = false;

    // Last-known main-viewport aspect ratio, refreshed every frame in
    // renderBuildPlate() -- needed by beginSwitchToMultiPlate() when
    // called from the toggle-view ribbon button, which (unlike the
    // double-click handler) runs outside renderBuildPlate()'s per-frame
    // scope and has no w/h of its own to compute aspect from.
    float m_lastViewportAspect = 16.0f / 9.0f;

    // Set on every frame the camera is mid-animateTo(); used to detect
    // the exact frame a transition settles, so we can log what the
    // camera actually ended up at -- confirms whether the transition
    // truly converges to what animateTo() was given, or something else
    // changes it afterward.
    bool m_wasCameraAnimating = false;

    // Main-viewport camera drag state. "Armed" means the button-down that
    // started the drag wasn't on top of a model part, decided once at
    // click time and held for the duration of that drag (re-testing every
    // frame would let a rotate flip into un-armed mid-drag if the cursor
    // happened to cross a part).
    bool m_vpOrbiting = false;
    bool m_vpOrbitArmed = false;
    bool m_vpPanning = false;

    // -----------------------------------------------------------------------
    // Main panel
    // -----------------------------------------------------------------------
    void renderSlicerPanel()
    {
        if (!m_panelOpen) return;

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::SetNextWindowSize(ImVec2(360, 700), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Slicer", &m_panelOpen, flags))
        {
            ImGui::End();
            return;
        }

        if (ImGui::Button("Slice Now"))
            m_sliceRequested = true;

        ImGui::Separator();
        ImGui::Spacing();

        renderSplitView();

        ImGui::End();
    }

    void renderBuildPlatePanel()
    {
        if (!m_panelOpen) return;

        ImGui::SetNextWindowSize(ImVec2(360, 700), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Build Plate", &m_panelOpen))
        {
            ImGui::End();
            return;
        }

        // render the 3d view from here.
        //m_buildPlateRenderer()
        //ImGui::BeginChild("##slicer_build", ImVec2(0, m_previewHeight), false);
        renderBuildPlate();
        //ImGui::EndChild();
        ImGui::End();
    }

    // -----------------------------------------------------------------------
    // Split view: tree top, properties bottom
    // -----------------------------------------------------------------------
    void renderSplitView()
    {
        const float splitterH = 6.0f;
        const float minTree = 60.0f;
        const float minProps = 60.0f;
        const float minPreview = 60.0f;
        const float avail = ImGui::GetContentRegionAvail().y;

        // Clamp tree height
        m_treeHeight = ImClamp(m_treeHeight, minTree, avail - minProps - minPreview - splitterH * 2);
        // Clamp preview height
        m_previewHeight = ImClamp(m_previewHeight, minPreview, avail - m_treeHeight - minProps - splitterH * 2);




        // ---- Tree ----
        ImGui::BeginChild("##slicer_tree", ImVec2(0, m_treeHeight), true,
            ImGuiWindowFlags_HorizontalScrollbar);

        //// Forward mouse wheel to this child when hovered
        //if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
        //{
        //    ImGui::SetScrollY(ImGui::GetScrollY() -
        //        ImGui::GetIO().MouseWheel * ImGui::GetTextLineHeight() * 3.0f);
        //}
        // Nuclear scroll fix — capture wheel before anything else consumes it
        ImGuiContext& g = *GImGui;
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
        {
            float wheel = g.IO.MouseWheel;
            if (wheel != 0.0f)
            {
                ImGuiWindow* window = ImGui::GetCurrentWindow();
                window->Scroll.y -= wheel * window->CalcFontSize() * 3.0f;
                window->Scroll.y = ImClamp(window->Scroll.y, 0.0f, window->ScrollMax.y);
            }
        }


        //m_meshCache.assets.clear();
        renderTree();
        ImGui::EndChild();

        // ---- Splitter: tree / properties ----
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
        ImGui::Button("##split1", ImVec2(-1, splitterH));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.y;
            float newTop = m_treeHeight + delta;
            if (newTop >= minTree && (avail - newTop - m_previewHeight - splitterH * 2) >= minProps)
                m_treeHeight = newTop;
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        // ---- Properties ----
        float propsHeight = avail - m_treeHeight - m_previewHeight - splitterH * 2;
        propsHeight = std::max(propsHeight, minProps);
        ImGui::BeginChild("##slicer_props", ImVec2(0, propsHeight), true);
        //// Forward mouse wheel to this child when hovered
        //if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
        //{
        //    ImGui::SetScrollY(ImGui::GetScrollY() -
        //        ImGui::GetIO().MouseWheel * ImGui::GetTextLineHeight() * 3.0f);
        //}
        renderProperties();
        ImGui::EndChild();

        // ---- Splitter: properties / preview ----
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));
        ImGui::Button("##split2", ImVec2(-1, splitterH));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.y;
            float newPreview = m_previewHeight - delta;
            if (newPreview >= minPreview && (avail - m_treeHeight - newPreview - splitterH * 2) >= minProps)
                m_previewHeight = newPreview;
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        // ---- Preview ----
        ImGui::BeginChild("##slicer_preview", ImVec2(0, m_previewHeight), false);
        //// Forward mouse wheel to this child when hovered
        //if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f)
        //{
        //    ImGui::SetScrollY(ImGui::GetScrollY() -
        //        ImGui::GetIO().MouseWheel * ImGui::GetTextLineHeight() * 3.0f);
        //}
        renderPreview();
        ImGui::EndChild();

    }

    // -----------------------------------------------------------------------
    // Tree
    // -----------------------------------------------------------------------
    void renderTree()
    {
        if (!m_workspaceStore)
        {
            ImGui::TextDisabled("No workspace store available.");
            return;
        }

        domain::v1::Project* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);

        if (!project)
        {
            ImGui::TextDisabled("No project loaded.");
            return;
        }
        ImGuiTreeNodeFlags rootFlags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        const std::string& projName = project->name.empty()
            ? "Project" : project->name;

        bool rootOpen = ImGui::TreeNodeEx("##proj_root", rootFlags);
        ImGui::SameLine();
        ImGui::TextUnformatted(projName.c_str());

        if (rootOpen)
        {

            bool rootOpen = ImGui::TreeNodeEx("##Build_plates", rootFlags);
            ImGui::SameLine();
            ImGui::TextUnformatted("Build Plates");

            for (auto& plate : project->buildPlates)
            {
                renderBuildPlates(plate);
            }
            ImGui::TreePop();


            for (auto& folder : project->projectFolders)
                renderFolder(folder);


            ImGui::TreePop();
        }
    }
    void renderBuildPlates(domain::v1::BuildPlate* plate)
    {
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGui::PushID(plate->Id.c_str());

        bool selected = (m_selectedPlateId > -1);

        std::string label = plate->name.c_str();


        if (ImGui::Selectable(label.c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns))
        {
            //find and render build plate

        }

        ImGui::PopID();
    }
    void renderFolder(ProjectFolder& folder)
    {
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_DefaultOpen |
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_SpanAvailWidth;

        ImGui::PushID(folder.Id);

        bool open = ImGui::TreeNodeEx("##f", flags);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.35f, 1.0f), "[F]");
        ImGui::SameLine();

        std::string label = folder.name;
        int total = folder.totalAssetCount();
        if (total > 0)
            label += "  (" + std::to_string(total) + ")";
        ImGui::TextUnformatted(label.c_str());

        //if (open)
        //{
        for (auto& asset : folder.importedAssets)
            renderAsset(*asset);

        for (auto& sub : folder.folders)
            renderFolder(sub);

        ImGui::TreePop();
        //}

        ImGui::PopID();
    }

    void renderAsset(const ImportedAsset& asset)
    {
        // Register in cache so properties panel can find it by idasset.Id.Id] = &asset;

        bool selected = s_selected.count(asset.Id) > 0;

        ImGui::PushID(asset.Id.c_str());

        // Indent to align with tree node labels
        float indent = ImGui::GetTreeNodeToLabelSpacing();
        ImGui::Indent(indent);

        // Icon — coloured by accent status
        ImVec4 iconCol = asset.isAccent
            ? ImVec4(0.40f, 0.80f, 1.00f, 1.0f)
            : ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
        ImGui::TextColored(iconCol, "[S]");
        ImGui::SameLine();

        // Build label
        std::string label = asset.name;
        if (asset.quantity > 1)
            label += "  x" + std::to_string(asset.quantity);
        if (asset.isAccent)
            label += "  [A]";

        // Selectable covers the full remaining width and handles click reliably
        if (ImGui::Selectable(label.c_str(), selected,
            ImGuiSelectableFlags_SpanAllColumns))
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl)
            {
                // Ctrl+click — toggle this item
                if (selected)
                    s_selected.erase(asset.Id);
                else
                    s_selected.insert(asset.Id);
            }
            else if (io.KeyShift && !s_selected.empty())
            {
                // Shift+click — range select (requires knowing render order)
                // Simple version: just add this item
                s_selected.insert(asset.Id);
            }
            else
            {
                // Plain click — select only this item
                s_selected.clear();
                s_selected.insert(asset.Id);
                //m_selectedAssetId = asset.Id;

                StlLoaderAdapter loader = StlLoaderAdapter(*m_modelCache);
                if (loader.load(asset.fileLocation))
                {
                    m_preview.loadMesh(loader.getMesh());


                }
            }


        }

        ImGui::Unindent(indent);
        ImGui::PopID();

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            std::vector<std::string> draggedAssetPointers;// = std::vector<std::shared_ptr<ImportedAsset>>;
            draggedAssetPointers.reserve(s_selected.size());
            std::string flattenedPayload = "";

            for (std::string selectedId : s_selected) {
                for (auto& assetItem : m_modelCache->assets) {
                    if (assetItem.first == selectedId) {
                        // Store the memory location of this specific asset instan-?ce
                        flattenedPayload = flattenedPayload + assetItem.second->fileLocation.c_str() + "\n";
                        break;
                    }
                }
            }

            size_t payloadSizeInBytes = draggedAssetPointers.size();
            ImGui::SetDragDropPayload("ASSET_PATHS", flattenedPayload.data(), flattenedPayload.size());

            if (s_selected.size() == 1) {
                ImGui::Text("Dragging: %s", flattenedPayload.c_str());
            }
            else {
                ImGui::Text("Dragging %d assets to Build Plate", (int)s_selected.size());
            }

            ImGui::EndDragDropSource();



        }
    }
    // -----------------------------------------------------------------------
    // Properties panel
    // -----------------------------------------------------------------------
    void renderProperties()
    {

        if (s_selected.size() == 0 || s_selected.size() > 1)
            //if (m_selectedAssetId < 0)
        {
            ImGui::TextDisabled("Select a file to view properties.");
            return;
        }
        if (s_selected.size() == 1)
        {
            std::string item = *s_selected.begin();

            auto it = m_modelCache->assets.find(item);
            if (it == m_modelCache->assets.end() || !it->second)
            {
                ImGui::TextDisabled("Selection no longer valid.");
                return;
            }

            domain::v1::ImportedAsset& asset = *it->second;

            sectionHeader("Asset Properties");

            // Editable name
            {
                char buf[256] = {};
                std::snprintf(buf, sizeof(buf), "%s", asset.name.c_str());
                if (ImGui::InputText("Name##n", buf, sizeof(buf)))
                    asset.name = buf;
            }

            // Quantity — writes back to asset directly
            ImGui::InputInt("Quantity##q", &asset.quantity);
            if (asset.quantity < 1) asset.quantity = 1;

            // Accent flag
            ImGui::Checkbox("Accent [A]##a", &asset.isAccent);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Read-only info
            readOnlyRow("Label", asset.label);
            readOnlyRow("File", asset.fileName);
            readOnlyRow("Location", asset.fileLocation);
            if (!asset.description.empty())
                readOnlyRow("Notes", asset.description);
        }


    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    void renderPreview()
    {
        // Use the child window's inner size — guaranteed non-zero since
        // we're inside BeginChild("##slicer_preview")
        ImVec2 avail = ImGui::GetContentRegionAvail();
        uint32_t w = (uint32_t)std::max(4.0f, avail.x);
        uint32_t h = (uint32_t)std::max(4.0f, avail.y);

        //printf("[Preview] renderPreview: w=%u h=%u loaded=%d\n",
        //    w, h, m_preview.isLoaded() ? 1 : 0);

        // Tick auto-rotation
        m_preview.paused = ImGui::IsWindowHovered();
        m_preview.tick(ImGui::GetIO().DeltaTime);

        if (m_preview.isLoaded())
        {
            m_preview.render(w, h);
            void* tex = m_preview.getTexture();
            //printf("[Preview] tex=%p\n", tex);
            if (tex)
            {
                ImGui::Image((ImTextureID)(intptr_t)tex,
                    ImVec2((float)w, (float)h),
                    ImVec2(0, 1), ImVec2(1, 0)); // flip UV Y
            }
            else
            {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No texture.");
            }
        }
        else
        {
            ImGui::TextDisabled("Select an asset to preview.");
        }
    }
    void renderGizmoOverlay(ImVec2 viewportTopLeft, ImVec2 avail)
    {
        const uint32_t gizmoSize = 200;
        const float pad = 14.0f;
        ImVec2 gizmoScreenPos(
            viewportTopLeft.x + avail.x - gizmoSize - pad,
            viewportTopLeft.y + pad);
        ImVec2 gizmoCentre(gizmoScreenPos.x + gizmoSize * 0.5f, gizmoScreenPos.y + gizmoSize * 0.5f);

        GLuint gizmoTex = m_viewportController->renderGizmoAndGetTexture(gizmoSize);

        ImGui::SetCursorScreenPos(gizmoScreenPos);
        if (gizmoTex)
            ImGui::Image((ImTextureID)(intptr_t)gizmoTex, ImVec2((float)gizmoSize, (float)gizmoSize), ImVec2(0, 1), ImVec2(1, 0));

        ImDrawList* dl = ImGui::GetWindowDrawList();

        auto* gizmo = m_viewportController->gizmo();
        struct LabelDef { glm::vec3 normal; const char* text; };
        LabelDef labels[] = {
            { { 0, 1, 0}, "TOP"  }, { { 0, 0, 1}, "FRONT"}, { { 1, 0, 0}, "RIGHT"},
            { { 0, 0,-1}, "BACK" }, { {-1, 0, 0}, "LEFT" }, { { 0,-1, 0}, "BTM"  },
        };
        const float faceS = 0.72f;
        glm::vec3 camFwd = gizmo->cameraForward();
        for (auto& lb : labels)
        {
            if (glm::dot(lb.normal, camFwd) >= -0.1f) continue;
            glm::vec2 local = gizmo->project2D(lb.normal * faceS, gizmoSize, gizmoSize);
            ImVec2 screenPt(gizmoScreenPos.x + local.x, gizmoScreenPos.y + local.y);
            ImVec2 ts = ImGui::CalcTextSize(lb.text);
            float bpad = 3.0f;
            dl->AddRectFilled(
                ImVec2(screenPt.x - ts.x * 0.5f - bpad, screenPt.y - ts.y * 0.5f - bpad),
                ImVec2(screenPt.x + ts.x * 0.5f + bpad, screenPt.y + ts.y * 0.5f + bpad),
                IM_COL32(0, 0, 0, 90), 3.0f);
            dl->AddText(ImVec2(screenPt.x - ts.x * 0.5f, screenPt.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 245), lb.text);
        }

        ImVec2 mousePos = ImGui::GetMousePos();
        ImGui::SetCursorScreenPos(gizmoScreenPos);
        ImGui::InvisibleButton("##camgizmo", ImVec2((float)gizmoSize, (float)gizmoSize));

        // Drag-to-orbit lives on the main viewport now (see renderBuildPlate())
        // -- the gizmo itself only handles click-to-snap-to-face and the
        // nudge arrows below.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            m_viewportController->handleGizmoClick(mousePos.x - gizmoScreenPos.x, mousePos.y - gizmoScreenPos.y, gizmoSize);
        }

        float arrowR = gizmoSize * 0.52f;
        struct Arrow { float angle, dYawRad, dPitchDeg; };
        Arrow arrows[] = {
            { 0.0f, 0.25f, 0.0f }, { glm::pi<float>(), -0.25f, 0.0f },
            { glm::half_pi<float>(), 0.0f, 15.0f }, { -glm::half_pi<float>(), 0.0f, -15.0f },
        };
        for (auto& arr : arrows)
        {
            float ax = gizmoCentre.x + std::cos(arr.angle) * arrowR;
            float ay = gizmoCentre.y - std::sin(arr.angle) * arrowR;
            float as = 9.0f, tipAngle = arr.angle + glm::pi<float>();
            ImVec2 tip(ax + std::cos(tipAngle) * as, ay - std::sin(tipAngle) * as);
            ImVec2 lft(ax + std::cos(tipAngle + glm::half_pi<float>()) * as * 0.5f, ay - std::sin(tipAngle + glm::half_pi<float>()) * as * 0.5f);
            ImVec2 rgt(ax + std::cos(tipAngle - glm::half_pi<float>()) * as * 0.5f, ay - std::sin(tipAngle - glm::half_pi<float>()) * as * 0.5f);
            bool hov = glm::length(glm::vec2(mousePos.x - ax, mousePos.y - ay)) < as * 1.5f;
            dl->AddTriangleFilled(tip, lft, rgt, hov ? IM_COL32(230, 230, 230, 255) : IM_COL32(160, 160, 165, 200));
            if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_viewportController->activeId() != "multi_plate_scene")
                m_viewportController->orbit(arr.dYawRad, glm::radians(arr.dPitchDeg));
        }
    }
    // editable_scene always re-centers whichever plate is active at world
    // origin, so its own default*() camera hooks are always the right
    // "come back to the single-plate view" target -- shared by the
    // toggle button and the multi-plate double-click handoff below.
    CameraState editableSceneDefaultCamera() const
    {
        CameraState c;
        c.target = m_editableScene->defaultTarget();
        c.distance = m_editableScene->defaultDistance();
        c.yaw = m_editableScene->defaultYaw();
        c.pitch = m_editableScene->defaultPitch();
        return c;
    }

    // Swaps to multi_plate_scene immediately and fades every OTHER plate
    // IN from invisible while the camera zooms out to frame all of them.
    // The plate editable_scene was just showing is forced fully opaque
    // immediately, not faded -- it was already on screen a moment ago,
    // so there's nothing to reveal there.
    //
    // Immediate ghost set (not deferred like beginSwitchToEditable()
    // below) because there's nothing for multi_plate_scene to fade out
    // from -- it isn't the active renderer yet, so nothing of it is on
    // screen to animate.
    void beginSwitchToMultiPlate()
    {
        if (!m_viewportController || !m_multiPlateScene || !m_editableScene) return;

        m_pendingSwitchToEditable = false;

        auto* activePlate = m_editableScene->sceneLayout().getActivePlate();
        std::string keepPlateId = activePlate ? activePlate->Id : std::string();

        m_multiPlateScene->sceneLayout().setImmediateGhostExcept(keepPlateId, 0.0f);
        m_viewportController->setActiveRenderer("multi_plate_scene");

        // editable_scene always shows its active plate re-centered at
        // world origin; multi_plate_scene (just swapped in) shows that
        // same plate at its own grid position instead. Snap the camera
        // target to match the instant the geometry reappears there, so
        // the zoom-out animates from where the plate actually now is,
        // not from the origin it was centered on a frame ago.
        glm::vec2 plateCenter;
        if (!keepPlateId.empty() && m_multiPlateScene->sceneLayout().tryGetPlateWorldCenter(keepPlateId, plateCenter))
            m_viewportController->setTarget(glm::vec3(plateCenter.x, 0.0f, plateCenter.y));

        CameraState fromCam = m_viewportController->camera();
        CameraState toCam = m_multiPlateScene->sceneLayout().computeOverviewCamera(fromCam.fovYRadians, m_lastViewportAspect);
        printf("[SlicerCorePlugin][toggle->multi] lastViewportAspect=%.4f fovY=%.4f\n", m_lastViewportAspect, fromCam.fovYRadians);
        printf("[SlicerCorePlugin][toggle->multi] FROM target=(%.2f,%.2f,%.2f) distance=%.2f yaw=%.4f pitch=%.4f\n",
            fromCam.target.x, fromCam.target.y, fromCam.target.z, fromCam.distance, fromCam.yaw, fromCam.pitch);
        printf("[SlicerCorePlugin][toggle->multi] TO   target=(%.2f,%.2f,%.2f) distance=%.2f yaw=%.4f pitch=%.4f\n",
            toCam.target.x, toCam.target.y, toCam.target.z, toCam.distance, toCam.yaw, toCam.pitch);

        m_viewportController->animateTo(toCam, 0.6f);
        m_multiPlateScene->sceneLayout().animateGhostAllTo(1.0f, 0.6f);
    }

    // Fades multi_plate_scene OUT while still active (so the fade is
    // actually visible), then hands off to editable_scene once that fade
    // completes -- see the m_pendingSwitchToEditable check in
    // renderBuildPlate(). targetPlate is the plate to make active in
    // editable_scene; pass nullptr to keep whichever plate is already
    // active there (the toggle button isn't switching plates, just views).
    //
    // The plate being handed off to stays fully opaque throughout --
    // only the OTHER plates fade out. That plate is where the camera is
    // headed and is exactly what editable_scene shows immediately after,
    // so making it disappear too would just be a needless flicker.
    void beginSwitchToEditable(domain::v1::BuildPlate* targetPlate)
    {
        if (!m_viewportController || !m_multiPlateScene || !m_editableScene) return;

        if (targetPlate)
        {
            m_navigation->setBuildPlate(targetPlate->Id);
            m_editableScene->sceneLayout().setActiveBuildPlate(targetPlate, *m_modelCache);
        }
        else
        {
            // Toggle-button case -- no plate was passed, so keep visible
            // whichever plate editable_scene is already showing.
            targetPlate = m_editableScene->sceneLayout().getActivePlate();
        }

        std::string keepPlateId = targetPlate ? targetPlate->Id : std::string();
        m_multiPlateScene->sceneLayout().animateGhostOutExcept(keepPlateId, 0.6f);

        // Camera eases toward the target plate's OWN grid position while
        // multi_plate_scene is still what's actually rendering -- it
        // hasn't been re-centered at the origin yet, that only happens
        // once editable_scene takes over (see the setTarget() snap in
        // renderBuildPlate() below). Using editable_scene's default
        // target (the origin) here would send the camera toward empty
        // space in the grid unless the plate happens to sit in the
        // origin cell.
        CameraState midFadeCamera = editableSceneDefaultCamera();
        glm::vec2 plateCenter;
        if (!keepPlateId.empty() && m_multiPlateScene->sceneLayout().tryGetPlateWorldCenter(keepPlateId, plateCenter))
            midFadeCamera.target = glm::vec3(plateCenter.x, 0.0f, plateCenter.y);

        m_viewportController->animateTo(midFadeCamera, 0.6f);
        m_pendingSwitchToEditable = true;
    }

    void renderBuildPlate()
    {
        m_buildPlateRenderer->tick(ImGui::GetIO().DeltaTime);

        // Debug: log the camera the instant an animateTo() transition
        // settles -- confirms whether it truly converges to what was
        // requested (see the FROM/TO log in beginSwitchToMultiPlate()),
        // or something changes it afterward. bool edge-detected (only
        // fires once per transition, on the animating->not-animating
        // frame), not every frame.
        if (m_viewportController)
        {
            bool animatingNow = m_viewportController->isCameraAnimating();
            if (m_wasCameraAnimating && !animatingNow)
            {
                const CameraState& c = m_viewportController->camera();
                printf("[SlicerCorePlugin][camera settle] target=(%.2f,%.2f,%.2f) distance=%.2f yaw=%.4f pitch=%.4f activeRenderer=%s\n",
                    c.target.x, c.target.y, c.target.z, c.distance, c.yaw, c.pitch, m_viewportController->activeId().c_str());
            }
            m_wasCameraAnimating = animatingNow;
        }

        // Keep the multi-plate view's active-plate highlight in sync
        // with whatever editable_scene is actually showing -- single
        // source of truth (EditableSceneLayout::getActivePlate()),
        // pulled fresh every frame here rather than pushed from each of
        // the several places that change the active plate (Arrange,
        // Load Workspace, startup, double-click), so there's no call
        // site that can forget and drift out of sync. setActivePlateId()
        // itself no-ops when the id hasn't changed.
        if (m_multiPlateScene && m_editableScene)
        {
            auto* activePlate = m_editableScene->sceneLayout().getActivePlate();
            m_multiPlateScene->sceneLayout().setActivePlateId(activePlate ? activePlate->Id : std::string());
        }

        // multi_plate_scene must stay the active renderer for its own
        // fade-out to be visible (see beginSwitchToEditable() above) --
        // so the actual renderer swap happens here, once that fade has
        // finished, rather than at the moment the toggle/double-click
        // was triggered.
        if (m_pendingSwitchToEditable && m_multiPlateScene && !m_multiPlateScene->sceneLayout().isGhostAnimating())
        {
            m_viewportController->setActiveRenderer("editable_scene");

            // multi_plate_scene was framing the target plate at its own
            // grid position (see beginSwitchToEditable() above);
            // editable_scene re-centers that same plate at world origin
            // the instant it becomes active, so the camera's target has
            // to snap there in this same frame to stay seamless --
            // distance/yaw/pitch already match (they were the eased
            // destination all along), only the target moves.
            if (m_editableScene) m_viewportController->setTarget(m_editableScene->defaultTarget());

            m_pendingSwitchToEditable = false;
        }

        bool showToolpathView = m_viewportController && m_viewportController->activeId() == "debug_comparison";
        ImVec2 viewportTopLeft = ImGui::GetCursorScreenPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();

        /*if (showToolpathView)
         {*/
        uint32_t w = (uint32_t)avail.x, h = (uint32_t)avail.y;
        if (h > 0) m_lastViewportAspect = (float)w / (float)h;
        GLuint tex = m_viewportController->renderAndGetTexture(w, h, ImGui::GetIO().DeltaTime, ImGui::IsWindowHovered());
        // NOT flipped -- unlike renderPreview()'s STL thumbnail and
        // renderGizmoOverlay()'s camera gizmo (both of which do need the
        // Y flip), the main viewport render was ALREADY correctly
        // oriented without one. A flip was tried here as a fix for the
        // raycast miss (theorizing the display was silently mirrored,
        // matching those other two textures) -- it visibly turned the
        // scene upside down instead, disproving that theory. Reverted;
        // the raycast bug is still unexplained, don't reintroduce this
        // without visual confirmation it's actually needed.
        if (tex) ImGui::Image((ImTextureID)(intptr_t)tex, avail);

        // ImGui::Image() registers with id=0, so IsItemHovered() on it
        // does NOT defer to items drawn on top of it later this frame
        // (gizmo buttons, the layer-range sliders, the debug checkboxes
        // below all sit visually on top of this same image). A plain
        // geometric rect test avoids that trap; the actual "is a *real*
        // widget on top of the cursor" check happens further down, once
        // every overlay for this frame has actually been submitted.
        bool imageAreaHovered = w > 0 && h > 0 &&
            ImGui::IsMouseHoveringRect(viewportTopLeft, ImVec2(viewportTopLeft.x + (float)w, viewportTopLeft.y + (float)h));

        bool leftClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool leftDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        float wheel = ImGui::GetIO().MouseWheel;

        RaycastHit clickHit, wheelHit;
        bool haveClickHit = false, haveWheelHit = false;

        if (imageAreaHovered && (leftClicked || wheel != 0.0f))
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            float px = mousePos.x - viewportTopLeft.x;
            float py = mousePos.y - viewportTopLeft.y;
            float ndcX = (px / (float)w) * 2.0f - 1.0f;
            // Flipped relative to the textbook screen-to-NDC formula
            // (1 - (py/h)*2) -- empirically confirmed by directly testing
            // both: flipping the main-viewport DISPLAY made every
            // raycast hit correctly, but visibly turned the render
            // upside down (reverted -- the render was already correctly
            // oriented on its own). This applies that same Y correction
            // only to the ray math instead, leaving the display alone --
            // whatever in this pipeline maps screen Y to render Y isn't
            // the naive relationship this formula assumed, but it's
            // still self-consistent for VIEWING (correct as rendered),
            // just not for the mouse-to-NDC math used here.
            float ndcY = -1.0f + (py / (float)h) * 2.0f;
            float aspect = (float)w / (float)h;

            auto camCtx = domain::v1::buildCameraContext(m_viewportController->camera(), aspect);

            glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
            glm::vec4 rayEye = glm::inverse(camCtx.proj) * rayClip;
            rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
            glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(camCtx.view) * rayEye));

            // Debug: log the double-click ray + resulting hit -- only in
            // the multi-plate view, per request, while tracking down a
            // build-plate hit-test miss. debugLogNextRaycast() makes the
            // raycast() call just below print per-plate diagnostics too.
            bool debugMultiPlateClick = leftDoubleClicked && m_multiPlateScene
                && m_viewportController->activeId() == "multi_plate_scene";
            if (debugMultiPlateClick)
            {
                const CameraState& cam = m_viewportController->camera();
                printf("[SlicerCorePlugin][dblclick] w=%u h=%u aspect=%.4f fovY=%.4f near=%.2f far=%.2f\n",
                    w, h, aspect, cam.fovYRadians, cam.nearPlane, cam.farPlane);
                printf("[SlicerCorePlugin][dblclick] camera target=(%.2f,%.2f,%.2f) distance=%.2f yaw=%.4f pitch=%.4f\n",
                    cam.target.x, cam.target.y, cam.target.z, cam.distance, cam.yaw, cam.pitch);
                printf("[SlicerCorePlugin][dblclick] mouse=(%.1f,%.1f) ndc=(%.3f,%.3f) rayOrigin=(%.2f,%.2f,%.2f) rayDir=(%.3f,%.3f,%.3f) camCtx.target=(%.2f,%.2f,%.2f)\n",
                    px, py, ndcX, ndcY, camCtx.camPos.x, camCtx.camPos.y, camCtx.camPos.z, rayWorld.x, rayWorld.y, rayWorld.z,
                    camCtx.target.x, camCtx.target.y, camCtx.target.z);

                // Independently mirror buildCameraContext()'s own basis
                // vectors here, rather than re-deriving them by hand from
                // yaw/pitch again -- ground truth, printed directly,
                // plus an orthonormality check and a reconstructed
                // dead-center-of-screen ray (which should equal `forward`
                // exactly, since off-axis basis vectors don't enter into
                // an ndcX=ndcY=0 unprojection at all).
                {
                    glm::vec3 dbgRight(sin(cam.yaw), 0.0f, -cos(cam.yaw));
                    glm::vec3 dbgForward = glm::normalize(cam.target - camCtx.camPos);
                    glm::vec3 dbgUp = -glm::normalize(glm::cross(dbgRight, dbgForward));
                    printf("[SlicerCorePlugin][dblclick] right=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f) forward=(%.3f,%.3f,%.3f)\n",
                        dbgRight.x, dbgRight.y, dbgRight.z, dbgUp.x, dbgUp.y, dbgUp.z, dbgForward.x, dbgForward.y, dbgForward.z);
                    printf("[SlicerCorePlugin][dblclick] orthocheck dot(right,up)=%.4f dot(right,fwd)=%.4f dot(up,fwd)=%.4f\n",
                        glm::dot(dbgRight, dbgUp), glm::dot(dbgRight, dbgForward), glm::dot(dbgUp, dbgForward));

                    glm::vec4 centerClip(0.0f, 0.0f, -1.0f, 1.0f);
                    glm::vec4 centerEye = glm::inverse(camCtx.proj) * centerClip;
                    centerEye = glm::vec4(centerEye.x, centerEye.y, -1.0f, 0.0f);
                    glm::vec3 centerRay = glm::normalize(glm::vec3(glm::inverse(camCtx.view) * centerEye));
                    printf("[SlicerCorePlugin][dblclick] screenCenterRay=(%.3f,%.3f,%.3f) (should equal forward above)\n",
                        centerRay.x, centerRay.y, centerRay.z);
                }

                m_multiPlateScene->sceneLayout().debugDumpPlatePositions();
                m_multiPlateScene->sceneLayout().debugLogNextRaycast();
            }

            if (leftClicked) { clickHit = m_viewportController->raycast(camCtx.camPos, rayWorld); haveClickHit = true; }
            if (wheel != 0.0f) { wheelHit = m_viewportController->raycast(camCtx.camPos, rayWorld); haveWheelHit = true; }

            if (debugMultiPlateClick)
            {
                if (clickHit.hit)
                    printf("[SlicerCorePlugin][dblclick] RESULT hit=1 plate=%s isPlateHit=%d point=(%.2f,%.2f,%.2f) dist=%.2f instanceId=%s\n",
                        clickHit.buildPlateId.c_str(), clickHit.isPlateHit ? 1 : 0,
                        clickHit.point.x, clickHit.point.y, clickHit.point.z, clickHit.distance, clickHit.instanceId.c_str());
                else
                    printf("[SlicerCorePlugin][dblclick] RESULT hit=0\n");
            }
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_vpOrbiting = false;
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) m_vpPanning = false;

        // Driven by IsMouseDown rather than re-checking hover every frame,
        // so a fast drag that momentarily leaves the viewport rect doesn't
        // interrupt the rotate/pan already in progress.
        //
        // Rotation is disabled in the multi-plate view -- it's an
        // overview at a fixed, purpose-fit angle (see
        // computeOverviewCamera()), and orbiting away from it both loses
        // that framing and was the exact scenario that caused the
        // "rotated the camera" confusion earlier in this feature's
        // development. Panning/zoom are left enabled -- only rotation is
        // disabled here, per request.
        if (m_vpOrbiting && m_vpOrbitArmed && m_viewportController->activeId() != "multi_plate_scene"
            && (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f))
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_viewportController->orbit(delta.x * 0.005f, -delta.y * 0.005f);
        }
        if (m_vpPanning && (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f))
        {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_viewportController->pan(delta.x, delta.y);
        }

        renderGizmoOverlay(viewportTopLeft, avail);

        if (m_viewportController->activeLayerCount() > 0)
        {
            //if (tex) ImGui::Image((ImTextureID)(intptr_t)tex, avail);



            m_viewportController->renderActiveUI(viewportTopLeft, avail);   // NEW — correct draw order, guaranteed on top

            int maxLayer = m_viewportController->activeLayerCount() - 1;
            static int startLayer = 0, endLayer = 0;

            ImVec2 scrubPos(viewportTopLeft.x + 8.0f, viewportTopLeft.y + 8.0f);
            ImVec2 scrubSize(20.0f, avail.y - 16.0f);

            ImGui::SetCursorScreenPos(scrubPos);
            ImGui::VSliderInt("##layerStart", scrubSize, &startLayer, 0, maxLayer, "%d");

            ImGui::SetCursorScreenPos(ImVec2(scrubPos.x + 28.0f, scrubPos.y));
            ImGui::VSliderInt("##layerEnd", scrubSize, &endLayer, 0, maxLayer, "%d");

            if (startLayer > endLayer) endLayer = startLayer;   // keep valid — start layer alone gives a single-layer view
            //    m_viewportController->setActiveVisibleLayer(startLayer);   // needs updating to a range-setter, see below
            //else
            m_viewportController->setActiveVisibleLayerRange(startLayer, endLayer);

            static bool showModel = true, showRibbon = true;

            ImVec2 debugPos(viewportTopLeft.x + 150.0f, viewportTopLeft.y + 8.0f);
            ImGui::SetCursorScreenPos(debugPos);

            if (ImGui::Checkbox("Show Mesh", &showModel))
                m_eventBus->publish("debug.toggle.model", showModel ? "1" : "0");

            ImGui::SameLine();

            if (ImGui::Checkbox("Show Ribbon", &showRibbon))
                m_eventBus->publish("debug.toggle.ribbon", showRibbon ? "1" : "0");

            ImVec2 debugPos2(viewportTopLeft.x + 150.0f, viewportTopLeft.y + 30.0f);
            ImGui::SetCursorScreenPos(debugPos2);
            static bool solidShader = false;
            if (ImGui::Checkbox("Solid Shading", &solidShader))
                m_eventBus->publish("debug.toggle.solidshader", solidShader ? "1" : "0");

        }

        // Everything that can visually sit on top of the viewport this
        // frame (gizmo, layer sliders, debug checkboxes) has now been
        // submitted, so IsAnyItemHovered() correctly reflects whether the
        // cursor is actually over one of THEM rather than bare viewport --
        // that's what was letting scroll/click on those widgets also
        // drive the camera before.
        bool blockedByOtherWidget = ImGui::IsAnyItemHovered();
        bool viewportInputAllowed = imageAreaHovered && !blockedByOtherWidget;

        if (viewportInputAllowed)
        {
            if (leftClicked)
            {
                m_vpOrbitArmed = !(haveClickHit && clickHit.hit && !clickHit.isPlateHit);   // hit a part -> don't arm orbit
                m_vpOrbiting = true;
            }
            if (rightClicked)
                m_vpPanning = true;
            if (wheel != 0.0f)
                m_viewportController->zoomToPoint(wheel * 10.0f, (haveWheelHit && wheelHit.hit) ? &wheelHit.point : nullptr);

            // Double-click a build plate (or a part sitting on one) in the
            // multi-plate overview -> switch to the editable single-plate
            // view for that plate, with the camera animating in. clickHit
            // is already populated above since leftClicked is true on the
            // same frame IsMouseDoubleClicked() fires. Only meaningful in
            // the multi-plate view -- editable_scene only ever has one
            // plate.
            //
            // The camera target here is editable_scene's own default, NOT
            // MultiPlateSceneLayout's grid position for this plate --
            // EditableSceneLayout::setActiveBuildPlate() always re-centers
            // whichever plate is active at world origin (see its
            // m_plateOffset derivation), independent of where that same
            // plate happened to sit in the grid a moment ago.
            if (leftDoubleClicked && haveClickHit && clickHit.hit && !clickHit.buildPlateId.empty()
                && m_viewportController->activeId() == "multi_plate_scene")
            {
                auto* project = m_navigation->resolveOrDefaultProject(*m_workspaceStore);
                domain::v1::BuildPlate* targetPlate = nullptr;
                if (project)
                {
                    for (auto* p : project->buildPlates)
                        if (p && p->Id == clickHit.buildPlateId) { targetPlate = p; break; }
                }

                if (targetPlate)
                {
                    beginSwitchToEditable(targetPlate);
                }
            }
        }


        /*   }
          else if (m_buildPlateRenderer->isLoaded())
          {
              m_buildPlateRenderer->setRegistry(this->m_registry);
              m_buildPlateRenderer->setViewportController(*m_viewportController);
              m_buildPlateRenderer->renderWindow();

              renderGizmoOverlay(viewportTopLeft, avail);
          }
          else
          {
              ImGui::TextDisabled("Select an asset to preview.");
          }*/
    }
    void sectionHeader(const char* title)
    {
        ImGui::Spacing();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float  w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(
            p0, ImVec2(p0.x + w, p0.y + 26.0f),
            ImGui::GetColorU32(ImVec4(1, 1, 1, 0.05f)), 3.0f);
        ImGui::SetCursorScreenPos(ImVec2(p0.x + 6.0f, p0.y + 5.0f));
        ImGui::TextUnformatted(title);
        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + 32.0f));
        ImGui::Spacing();
    }

    void readOnlyRow(const char* key, const std::string& value)
    {
        if (value.empty()) return;
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(key);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::Spacing();
    }
};




// -----------------------------------------------------------------------
// Required exports
// -----------------------------------------------------------------------
extern "C" __declspec(dllexport)
IUiModule* pistachio_create_ui_module() { return new SlicerCorePlugin(); }

extern "C" __declspec(dllexport)
void pistachio_destroy_ui_module(IUiModule* m) { delete m; }

static const UiPluginManifestV1 g_manifest = {
    sizeof(UiPluginManifestV1), 1,
    "pistachio.slicer_core", "Pistachio Slicer Core", "0.1.0", "Slicer"
};

extern "C" __declspec(dllexport)
const UiPluginManifestV1* pistachio_get_ui_manifest() { return &g_manifest; }
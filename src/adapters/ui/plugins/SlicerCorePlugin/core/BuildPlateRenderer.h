#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "Project.h"
#include "adapters/ui/ContributionRegistry.h"
#include "SelectionManager.h"
#include "domain/buildplate.h"

#include <glad/glad.h>
#include <chrono>    

#include "domain/RenderModel.h"
#include "domain/raycastHit.h"
#include "NavigationManager.h"
#include "adapters/rendering/ViewportController.h"
#include "ports/I3DViewportGLRender.h"

namespace slicer {

   
    struct SceneBounds {
        glm::vec3 min{ FLT_MAX,  FLT_MAX,  FLT_MAX };
        glm::vec3 max{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

        void reset() {
            min = glm::vec3(FLT_MAX);
            max = glm::vec3(-FLT_MAX);
        }

        void grow(const BoundingBox& box) {
            min = glm::min(min, box.min);
            max = glm::max(max, box.max);
        }

        glm::vec3 getCenter() const {
            return (max + min) * 0.5f;
        }

        float getRadius() const {
            return glm::distance(max, min) * 0.5f;
        }
    };

    struct Grid
    {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        int lineCount = 0;

        glm::vec3 center{ 0.0f };
        float size = 0.0f;
    };

    class GlMesh
    {
    public:

        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;

        uint32_t indexCount = 0;

        glm::vec3 center;
        float radius = 1.0f;


        BoundingBox bounds;

        bool create(std::shared_ptr<Model> model);
        void destroy();
        void render() const;
    };

    //// Raycast hit result structure
    //struct RaycastHit {
    //    bool hit = false;
    //    std::string instanceId;
    //    class RenderModel* renderModel = nullptr;
    //    std::shared_ptr<domain::v1::Model> model = nullptr;
    //    glm::vec3 point;
    //    glm::vec3 normal;
    //    float distance = std::numeric_limits<float>::max();
    //    uint32_t triangleIndex = 0;
    //    std::string buildPlateId;    
    //    bool isPlateHit = false;

    //};


    // Add this class before SlicerCorePlugin
    class TreeViewRenderer {
    public:
        TreeViewRenderer(NavigationManager* navigation, domain::v1::WorkspaceStore* workspaceStore, ModelCache* cache);
        ~TreeViewRenderer();

        void render();
 
        void renderBuildPlateNode(domain::v1::BuildPlate* buildPlate,
            const std::unordered_set<std::string>& selectedIds,
            ImGuiTreeNodeFlags baseFlags);
        

        void renderInstanceNode(ModelInstance* instance,
            const std::unordered_set<std::string>& selectedIds,
            ImGuiTreeNodeFlags baseFlags);

    private:
        NavigationManager* m_navigation;
        domain::v1::WorkspaceStore* m_workspaceStore;
        ModelCache* m_cache;
    };

    class BoundingBoxRenderer
    {
    public:
        BoundingBoxRenderer();
        ~BoundingBoxRenderer();

        void create();   // builds 8 verts + 24 indices (12 line segments) for a unit cube, once
        void render(const glm::mat4& mvp, glm::vec3 color, GLuint lineShader) const;
    private:
        GLuint vao = 0, vbo = 0, ebo = 0;
        uint32_t indexCount = 0;
    };


    

    /*struct CameraState {
        glm::vec3 target;
        float distance;
        float yaw;
        float pitch;
    };*/

    struct PlateEntry {
        glm::vec2 center;
        int plateIndex;
    };

    class SceneLayout {

    private:
        void bestGrid(int nItems, int aspectW, int aspectH, int& bestCols, int& bestRows);
        void rebuildSceneBounds();
        CameraState computeTargetCamera(int plateIndex) const; // -1 = overview
        

        SceneBounds m_fullSceneBounds; // full scene, never changes after createLayout

        CameraState  m_cameraFrom;
        CameraState  m_cameraTo;
        CameraState  m_cameraCurrent;
        float        m_transitionT = 1.0f;  // 1.0 = idle/complete
        float        m_transitionDuration = 0.6f;
        int          m_selectedPlate = -1;

        std::vector<std::shared_ptr<RenderModel>> m_renderModels;
        bool m_singleBuildPlateMode;
        SceneBounds m_sceneBounds;

        // per-plate ghost factors, indexed same as m_plateEntries
        std::vector<float> m_ghostFactors;
        std::vector<float> m_ghostTargets;

        // defaults
        float m_defaultYaw = 4.71239f;
        float m_defaultPitch = 45.0f;
        float m_itemWidth = 350.0f;
        float m_itemHeight = 350.0f;


    public:
        SceneLayout();
        ~SceneLayout();
        
        RenderModel* getRenderModel(const std::string& instanceId) const;
        void adjustZoom(float delta);
        void update(float dt);          
        std::vector<PlateEntry>  m_plateEntries;   // cached centers from createLayout
        std::vector<std::shared_ptr<RenderModel>> m_plateRenderModels;
 
        void setYawPitch(float yaw, float pitch);
        bool isAnimating() const;
        float getPlateGhostFactor(int plateIndex) const;

        void selectPlate(int index);   // -1 = overview
        void createLayout(std::vector<domain::v1::BuildPlate*> buildPlates, ModelCache& cache);
 
        CameraState getCurrentCamera() const;
        void preRender();
        std::vector<std::shared_ptr<RenderModel>> getRenderModels();
        SceneBounds getSceneBounds();

    };

    struct GizmoVertex { glm::vec3 pos, nrm; glm::vec2 uv; };

    // Returns verts + indices for one quad face (two triangles)
    static void pushQuad(std::vector<GizmoVertex>& verts,
        std::vector<uint32_t>& idx,
        glm::vec3 a, glm::vec3 b,
        glm::vec3 c, glm::vec3 d,
        glm::vec3 normal)
    {
        uint32_t base = (uint32_t)verts.size();
        verts.push_back({ a, normal, {0,0} });
        verts.push_back({ b, normal, {1,0} });
        verts.push_back({ c, normal, {1,1} });
        verts.push_back({ d, normal, {0,1} });
        idx.insert(idx.end(), { base,base + 1,base + 2, base,base + 2,base + 3 });
    }

    static void pushTri(std::vector<GizmoVertex>& verts,
        std::vector<uint32_t>& idx,
        glm::vec3 a, glm::vec3 b, glm::vec3 c,
        glm::vec3 normal)
    {
        uint32_t base = (uint32_t)verts.size();
        verts.push_back({ a, normal, {0,0} });
        verts.push_back({ b, normal, {0.5f,1} });
        verts.push_back({ c, normal, {1,0} });
        idx.insert(idx.end(), { base,base + 1,base + 2 });
    }


    class BuildPlateRenderer
    {
    public:

        BuildPlateRenderer();

        ~BuildPlateRenderer();

        void initialize(domain::v1::WorkspaceStore& workspaceStore, domain::v1::Project* project,ModelCache& cache, NavigationManager& navigationManager);
        
 
        void updateViewModel(std::vector<domain::v1::BuildPlate*> buildPlates);
        void setViewportController(ViewportController& controller) { m_viewportController = &controller; }
        void clear();
        void tick(float dtSeconds);
        float autoRotateSpeed = 0.785f; // ~pi/4 per second

        void render(uint32_t width, uint32_t height);
        void renderOpenGL(uint32_t width, uint32_t height);

        void renderWindow();
        void* getTexture() const;
         

        bool isLoaded() { return m_isLoaded; }

        void setRegistry(adapters::ContributionRegistry* registry)
        {
            m_registry = registry;
        }

        // Selection methods
        RaycastHit raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);
         void deselectAll();
        void  selectInstance(const std::string& instanceId, bool additive);
        RenderModel* getSelectedModel() const;
        void handleMouseClick(float mouseX, float mouseY, uint32_t width, uint32_t height);
        
        SceneLayout& getSceneLayout() { return m_sceneLayout; }
        //SelectionManager& getSelectionManager() { return m_selectionManager; }

    private:

        ViewportController* m_viewportController = nullptr;

        GLuint m_singleIconTexture = 0;
        GLuint m_multiIconTexture = 0;
        bool m_multiBuildPlateView = false;
        bool m_treeHovered = false;
        BoundingBoxRenderer m_boundingBoxRenderer;
        GLuint m_lineShader = 0;
         
        domain::v1::WorkspaceStore* m_workspaceStore;

        // header, private section
        float m_fpsAccumTime = 0.0f;
        int   m_fpsFrameCount = 0;
        float m_fpsDisplay = 0.0f;

        bool m_isLoaded = false;
        //SelectionManager m_selectionManager;

        std::vector<domain::v1::BuildPlate*> m_buildPlates;
        std::vector<std::shared_ptr<RenderModel>> m_plateRenderModels;
        // in BuildPlateRenderer private members:
        std::chrono::steady_clock::time_point m_lastFrameTime = std::chrono::steady_clock::now();


        uint32_t m_gizmoIndexCount = 0;
        int m_gizmoHoveredFace = -1;
        // Per-face draw ranges (offset, count, normal, color, label)
        struct GizmoFace {
            uint32_t  idxOffset;
            uint32_t  idxCount;
            glm::vec3 normal;
            glm::vec3 color;
            const char* label;      // nullptr = edge/corner
            float     snapYaw;
            float     snapPitch;
        };
        std::vector<GizmoFace> m_gizmoFaces;
        
        // Gizmo FBO
        GLuint m_gizmoFbo = 0;
        GLuint m_gizmoColor = 0;
        GLuint m_gizmoDepth = 0;
        uint32_t m_gizmoFboW = 0;
        uint32_t m_gizmoFboH = 0;

        // Gizmo GL objects
        GLuint m_gizmoProgram = 0;
        GLuint m_gizmoVao = 0;
        GLuint m_gizmoVbo = 0;
        GLuint m_gizmoEbo = 0;

        GLuint m_bgProgram = 0;
        GLuint m_bgVao = 0;
        GLuint m_bgVbo = 0;

        void ensureBackgroundQuad();
        void ensureGizmoGl();
        void ensureGizmoFbo(uint32_t w, uint32_t h);
        void renderGizmoFbo(uint32_t w, uint32_t h);
        void renderCameraGizmo();

         adapters::ContributionRegistry* m_registry = nullptr;
        bool windowOpen = false;

        ModelCache* m_modelCache = nullptr; // the 3d mesh of the stls that have been loaded
         
        std::unique_ptr<TreeViewRenderer> m_treeViewRenderer;
        // ---- GL resource handles ----
        unsigned int m_meshVao = 0;
        unsigned int m_meshVbo = 0;
        unsigned int m_meshProgram = 0;
        int          m_meshIndexCount = 0;

        unsigned int m_gridVao = 0;
        unsigned int m_gridVbo = 0;
        unsigned int m_gridProgram = 0;
        int          m_gridLineCount = 0;

        unsigned int m_fbo = 0;
        unsigned int m_fboColor = 0;
        unsigned int m_fboDepth = 0;
        uint32_t     m_fboWidth = 0;
        uint32_t     m_fboHeight = 0;

        // ---- Mesh state ----
        bool      m_glReady = false;
        float     m_meshRadius = 40.0f;   // bounding sphere radius after centering
        // ----- Build Plate state -----

 
        

        // ---- Selection ----
        RenderModel* m_selectedModel = nullptr;

        // ---- Camera ----
        float m_yaw = 4.71239f;// 1.0f;// 0.785f;   // auto-rotates
        float m_pitch = 45.0f;    // fixed elevation (~25 deg)
        float m_distance = 3.0f;     // updated after load

        

        NavigationManager* m_navigation = nullptr;;

        SceneLayout m_sceneLayout;

        glm::mat4 m_view{ 1 };
        glm::mat4 m_proj{ 1 };
        glm::vec3 m_camPos{ 0,0,3 };  // kept in sync with updateCamera

        // ---- Internal ----
        bool ensureGl();
        void createShaders();
        
        void ensureFbo(uint32_t w, uint32_t h);
        void updateCamera(uint32_t w, uint32_t h);
       
        void DrawViewportToolbar(GLuint singleIconTexture,
            GLuint multiIconTexture,
            bool multiBuildPlateView);

        float m_lastMouseX = 0.0f;
        float m_lastMouseY = 0.0f;
        bool m_mouseHovering = false;

        GLuint m_singleIcon;
        GLuint m_multiIcon;

      
        void renderGrid();
        void destroyGl();

        static unsigned int compileShader(unsigned int type, const char* src);
        static unsigned int linkProgram(unsigned int vs, unsigned int fs);
    };
}
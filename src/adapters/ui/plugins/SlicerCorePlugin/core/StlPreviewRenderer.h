#pragma once

// -----------------------------------------------------------------------
// StlPreviewRenderer.h
//
// Standalone OpenGL renderer for the slicer plugin.
// - Loads an STL file into a GPU mesh
// - Auto-centres and scales the mesh to fit the view
// - Auto-rotates 360 degrees around Y axis (carousel)
// - Renders a flat grid bedplate underneath
// - Renders to an offscreen FBO; call getTexture() for ImGui::Image()
//
// No dependency on ports::IRendererPort or any host renderer.
// Place at: src/adapters/ui/plugins/SlicerCorePlugin/core/StlPreviewRenderer.h
// -----------------------------------------------------------------------

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "domain/DataContext.h"
 

namespace slicer {

    class StlPreviewRenderer
    {
    public:
        StlPreviewRenderer();
        ~StlPreviewRenderer();

		bool initialize(ModelCache& cache);

        // Load an mesh into GPU — call when asset is selected.
        // Returns false if the file could not be loaded.
        bool loadMesh(std::shared_ptr<domain::v1::Mesh> mesh);

        // Unload current mesh and show empty scene.
        void clear();

        // Call every frame before rendering.
        // dtSeconds — frame delta time for auto-rotation.
        void tick(float dtSeconds);

        // Render to the internal FBO at the given size.
        // Call every frame while the preview is visible.
        void render(uint32_t width, uint32_t height);

        // Returns the FBO colour texture as ImTextureID.
        // Valid after the first render() call.
        void* getTexture() const;

        // Auto-rotate speed in radians per second (default: one full turn per 8s)
        float autoRotateSpeed = 0.785f; // ~pi/4 per second

        // Pause auto-rotation while the user is hovering the preview
        bool  paused = false;

        bool  isLoaded() const { return m_meshLoaded; }

    private:
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
        bool      m_meshLoaded = false;
        glm::vec3 m_meshCenter = { 0,0,0 };
        float     m_meshRadius = 1.0f;   // bounding sphere radius after centering

        // ---- Camera / animation ----
        float m_yaw = 0.785f;   // auto-rotates
        float m_pitch = 0.85f;    // fixed elevation (~25 deg)
        float m_distance = 3.0f;     // updated after load

        glm::mat4 m_view{ 1 };
        glm::mat4 m_proj{ 1 };
        glm::vec3 m_camPos{ 0,0,3 };  // kept in sync with updateCamera

        // ---- Internal ----
        bool ensureGl();
        void createShaders();
        void createGrid(float size, int divisions);
        void ensureFbo(uint32_t w, uint32_t h);
        void updateCamera(uint32_t w, uint32_t h);
        void renderMesh();
        void renderGrid();
        void destroyGl();

        static unsigned int compileShader(unsigned int type, const char* src);
        static unsigned int linkProgram(unsigned int vs, unsigned int fs);
    };

} // namespace slicer
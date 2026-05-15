#pragma once

#include "ports/IRendererPort.h"
#include "adapters/rendering/FramebufferManager.h"

// IMPORTANT:
// Do NOT include Dear ImGui's OpenGL loader header (imgui_impl_opengl3_loader.h)
// from this header.
//
// This file is included by main.cpp, and pulling the loader into that
// translation unit has caused build failures on Windows due to include-order
// sensitivities and missing GL type definitions.
//
// We include the GL loader ONLY inside GlCubeViewRenderer.cpp.

#include <glm/glm.hpp>
#include <memory>

namespace adapters {

    // A simple OpenGL renderer adapter that draws a shaded 3D cube directly to the host backbuffer.
    // Designed to act as the "full-screen 3D view" behind ImGui floating windows.
    //
    // Notes:
    // - Does NOT install any GLFW callbacks (input stays managed by ImGuiHost's chained callbacks).
    // - Camera controls are provided via rotate/pan/zoom for later UI wiring.
    class GlCubeViewRenderer final : public ports::IRendererPort {
    public:
        GlCubeViewRenderer();
        ~GlCubeViewRenderer() override;

        bool initialize() override;
        void shutdown() override;

        // Render to the currently active OpenGL context (expects the caller's GLFW window/context).
        // ports::IRendererPort
        void render(GLFWwindow* window) override;

        void renderToFramebuffer(void* nativeWindow, uint32_t width, uint32_t height) override;

        // Model rendering is not supported yet (cube only)
        void setModel(std::shared_ptr<domain::Model> model) override;
        void fitAll() override;

        // Embedded texture viewport not supported (we draw to backbuffer)
        void* getFramebufferTexture() override;
        void resize(int width, int height) override;

        void setScene(const ports::RenderScene& scene) override;
        void setCameraState(const ports::CameraState& camera) override;
        ports::CameraState getCameraState() const override;
        ports::RendererCapabilities getCapabilities() const override;
        ports::RendererBackend getBackend() const override;

        void rotate(float dx, float dy) override;
        void pan(float dx, float dy) override;
        void zoom(float delta) override;
        void setViewDirection(int direction) override;

    private:
        void createResources();
        void destroyResources();
        void updateDerivedCamera();
        void ensureFramebuffer(uint32_t width, uint32_t height);

        // GL resources
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        unsigned int m_ebo = 0;
        unsigned int m_program = 0;

        // Cached viewport size (backbuffer size)
        int m_viewWidth = 1;
        int m_viewHeight = 1;

        // Scene/camera
        ports::RenderScene m_scene{};
        ports::CameraState m_camera{};

        // Orbit camera derived values
        float m_yaw = 0.785f;      // 45° - classic 3/4 view
        float m_pitch = 0.615f;    // ~35° - shows top nicely
        float m_distance = 4.0f;

         
        // Demo scene sizing (used to keep zoom/clipping sane)
        float m_modelScale = 1.0f; // world units (cube is scaled by this)
        float m_objectRadius = 1.0f; // computed from modelScale

        // Offscreen framebuffer for ImGui viewport (ImGui::Image)
        FramebufferManager m_framebuffer;
        uint32_t m_fbWidth = 0;
        uint32_t m_fbHeight = 0;
        bool m_fbInitialized = false;

        glm::mat4 m_view{ 1.0f };
        glm::mat4 m_proj{ 1.0f };
    };

} // namespace adapters
#pragma once

#include "ports/IRendererPort.h"

#include <cstdint>

// Use ImGui's OpenGL loader so we don't add a second GL loader to the build.
#include <imgui_impl_opengl3_loader.h>

namespace adapters {

    // A lightweight, embedded OpenGL renderer that draws a renderer-agnostic RenderScene
    // into an FBO texture for display in the ImGui viewport.
    //
    // This is intentionally minimal: it does not render OCCT BReps yet. The goal is to
    // provide a second renderer adapter behind the same port so you can feature-toggle
    // between OCCT (native CAD view) and OpenGL (embedded view).
    class OpenGlRenderer final : public ports::IRendererPort {
    public:
        OpenGlRenderer();
        ~OpenGlRenderer() override;

        bool initialize() override;
        void shutdown() override;
        void render() override;

        void setModel(std::shared_ptr<domain::Model> model) override;
        void fitAll() override;

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
        bool m_initialized = false;
        int m_width = 800;
        int m_height = 600;

        std::shared_ptr<domain::Model> m_model;
        ports::RenderScene m_scene;
        ports::CameraState m_camera;

        // GL resources
        GLuint m_fbo = 0;
        GLuint m_colorTex = 0;
        GLuint m_depthRbo = 0;
        GLuint m_vao = 0;
        GLuint m_vbo = 0;
        GLuint m_program = 0;

        void recreateFramebuffer();
        void destroyFramebuffer();
        void destroyDrawResources();

        void ensureProgram();
        void drawWorkplaneGrid();
        void drawScenePrimitives();
        static glm::mat4 makeView(const ports::CameraState& c);
        static glm::mat4 makeProj(const ports::CameraState& c, float aspect);
    };

} // namespace adapters

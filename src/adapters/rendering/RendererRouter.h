#pragma once

#include "ports/IRendererPort.h"

#include <memory>

namespace adapters {

    // A thin adapter that routes all IRendererPort calls to the currently active backend.
    // This lets you feature-toggle between multiple renderer implementations at runtime.
    class RendererRouter final : public ports::IRendererPort {
    public:
        RendererRouter(std::unique_ptr<ports::IRendererPort> occt,
                       std::unique_ptr<ports::IRendererPort> openGl);

        // In RendererRouter.h - add to public methods:
        ports::IRendererPort* getOcctRenderer();
        ports::IRendererPort* getOpenGlRenderer();

        bool initialize() override;
        void shutdown() override;
        void render(GLFWwindow* m_window) override;
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

        // Router-only API (safe to call via dynamic_cast from UI)
        void setActiveBackend(ports::RendererBackend backend);
        ports::RendererBackend getActiveBackend() const;

    private:
        std::unique_ptr<ports::IRendererPort> m_occt;
        std::unique_ptr<ports::IRendererPort> m_openGl;
        ports::RendererBackend m_active = ports::RendererBackend::Occt;

        // Sticky data we forward to both backends so switching is seamless
        std::shared_ptr<domain::Model> m_model;
        ports::RenderScene m_scene;
        ports::CameraState m_camera;
        int m_width = 800;
        int m_height = 600;

        ports::IRendererPort* active();
        const ports::IRendererPort* active() const;
        void pushStateTo(ports::IRendererPort* r);
    };

} // namespace adapters

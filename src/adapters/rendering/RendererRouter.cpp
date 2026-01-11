#include "adapters/rendering/RendererRouter.h"

namespace adapters {

    RendererRouter::RendererRouter(std::unique_ptr<ports::IRendererPort> occt,
                                   std::unique_ptr<ports::IRendererPort> openGl)
        : m_occt(std::move(occt)), m_openGl(std::move(openGl)) {
    }

    // In RendererRouter.h - add to public methods:
    ports::IRendererPort* getOcctRenderer() { return m_occt.get(); }
    ports::IRendererPort* getOpenGlRenderer() { return m_openGl.get(); }


    ports::IRendererPort* RendererRouter::active() {
        return (m_active == ports::RendererBackend::OpenGL) ? m_openGl.get() : m_occt.get();
    }

    const ports::IRendererPort* RendererRouter::active() const {
        return (m_active == ports::RendererBackend::OpenGL) ? m_openGl.get() : m_occt.get();
    }

    void RendererRouter::pushStateTo(ports::IRendererPort* r) {
        if (!r) return;
        r->resize(m_width, m_height);
        if (m_model) r->setModel(m_model);
        r->setScene(m_scene);
        r->setCameraState(m_camera);
    }

    bool RendererRouter::initialize() {
        bool ok = true;
        if (m_occt) ok = ok && m_occt->initialize();
        if (m_openGl) ok = ok && m_openGl->initialize();
        // Push state to both after init (important for OpenGL FBO sizing)
        pushStateTo(m_occt.get());
        pushStateTo(m_openGl.get());
        return ok;
    }

    void RendererRouter::shutdown() {
        if (m_openGl) m_openGl->shutdown();
        if (m_occt) m_occt->shutdown();
    }

    void RendererRouter::render() {
        if (auto* r = active()) r->render();
    }

    void RendererRouter::setModel(std::shared_ptr<domain::Model> model) {
        m_model = std::move(model);
        if (m_occt) m_occt->setModel(m_model);
        if (m_openGl) m_openGl->setModel(m_model);
    }

    void RendererRouter::fitAll() {
        if (m_occt) m_occt->fitAll();
        if (m_openGl) m_openGl->fitAll();
    }

    void* RendererRouter::getFramebufferTexture() {
        if (auto* r = active()) return r->getFramebufferTexture();
        return nullptr;
    }

    void RendererRouter::resize(int width, int height) {
        m_width = width;
        m_height = height;
        if (m_occt) m_occt->resize(width, height);
        if (m_openGl) m_openGl->resize(width, height);
    }

    void RendererRouter::setScene(const ports::RenderScene& scene) {
        m_scene = scene;
        if (m_occt) m_occt->setScene(m_scene);
        if (m_openGl) m_openGl->setScene(m_scene);
    }

    void RendererRouter::setCameraState(const ports::CameraState& camera) {
        m_camera = camera;
        if (m_occt) m_occt->setCameraState(m_camera);
        if (m_openGl) m_openGl->setCameraState(m_camera);
    }

    ports::CameraState RendererRouter::getCameraState() const {
        if (auto* r = active()) return r->getCameraState();
        return m_camera;
    }

    ports::RendererCapabilities RendererRouter::getCapabilities() const {
        if (auto* r = active()) return r->getCapabilities();
        return {};
    }

    ports::RendererBackend RendererRouter::getBackend() const {
        if (auto* r = active()) return r->getBackend();
        return m_active;
    }

    void RendererRouter::rotate(float dx, float dy) {
        if (auto* r = active()) r->rotate(dx, dy);
    }

    void RendererRouter::pan(float dx, float dy) {
        if (auto* r = active()) r->pan(dx, dy);
    }

    void RendererRouter::zoom(float delta) {
        if (auto* r = active()) r->zoom(delta);
    }

    void RendererRouter::setViewDirection(int direction) {
        if (auto* r = active()) r->setViewDirection(direction);
    }

    void RendererRouter::setActiveBackend(ports::RendererBackend backend) {
        if (backend == m_active) return;
        m_active = backend;
        // Ensure the newly active renderer has up-to-date state.
        pushStateTo(active());
    }

    ports::RendererBackend RendererRouter::getActiveBackend() const {
        return m_active;
    }

} // namespace adapters

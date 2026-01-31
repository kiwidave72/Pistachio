#pragma once

struct GLFWwindow;

namespace adapters {

    // Owns the single GLFW window + OpenGL context for the application.
    //
    // Why this exists:
    // - ImGui (platform backend) needs a window/context.
    // - Your embedded OpenGL renderer needs a current context when it renders the FBO.
    // - Hot-reloading the UI plugin must NOT create/destroy contexts; the host stays stable.
    class ContextManager {
    public:
        ContextManager() = default;
        ~ContextManager();

        ContextManager(const ContextManager&) = delete;
        ContextManager& operator=(const ContextManager&) = delete;

        // If there's already a current GLFW context, adopt it.
        // Otherwise, create a host window/context.
        bool initialize();
        void shutdown();

        GLFWwindow* window() const { return m_window; }

        void makeCurrent() const;
        void swapBuffers() const;
        void pollEvents() const;
        bool shouldClose() const;

    private:
        GLFWwindow* m_window = nullptr;
        bool m_ownsWindow = false;
        bool m_initedGlfw = false;
    bool m_glLoaded = false;
    };

} // namespace adapters

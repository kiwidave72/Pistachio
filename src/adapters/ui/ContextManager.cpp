#include "adapters/ui/ContextManager.h"

#include <cstdio>

// Prevent GLFW from including legacy <GL/gl.h> (OpenGL 1.1 only on Windows).
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Use Dear ImGui's OpenGL loader (GL3W) shipped with your imgui-fork.
// This provides modern GL enums and function pointers (FBOs, shaders, VAOs, etc.).
#include "adapters/rendering/OpenGLApi.h"

namespace adapters {

    static GLFWwindow* CreateHostWindow(bool& outGlfwInitedByHost)
    {
        if (!glfwInit())
            return nullptr;

        outGlfwInitedByHost = true;

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* w = glfwCreateWindow(1280, 800, "Pistachio", nullptr, nullptr);
        if (!w)
            return nullptr;

        glfwMakeContextCurrent(w);
        // Load GL3+ entry points once a context is current.
        // NOTE: imgl3wInit2 is provided by imgui_impl_opengl3_loader.h.
        if (imgl3wInit2((GL3WGetProcAddressProc)glfwGetProcAddress) != 0) {
            std::printf("[ContextManager][WARN] imgl3wInit2 failed; OpenGL 3+ entry points may be missing.\n");
        }
        glfwSwapInterval(1);
        return w;
    }

    ContextManager::~ContextManager() {
        shutdown();
    }

    bool ContextManager::initialize() {
        if (m_window)
            return true;

        // 1) Prefer an already-current context.
        m_window = glfwGetCurrentContext();
        if (m_window) {
            // Adopt existing context; ensure GL loader is initialized.
            if (!m_glLoaded) {
                if (imgl3wInit2((GL3WGetProcAddressProc)glfwGetProcAddress) != 0) {
                    std::printf("[ContextManager][WARN] imgl3wInit2 failed; OpenGL 3+ entry points may be missing.\n");
                }
                m_glLoaded = true;
            }
            return true;
        }

        std::printf("[ContextManager] No current GLFW context. Creating host window...\n");
        m_window = CreateHostWindow(m_initedGlfw);
        if (!m_window)
            return false;
        m_ownsWindow = true;
        m_glLoaded = true;
        return true;
    }

    void ContextManager::shutdown() {
        if (m_ownsWindow && m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            m_ownsWindow = false;
        }

        if (m_initedGlfw) {
            glfwTerminate();
            m_initedGlfw = false;
        }
    }

    void ContextManager::makeCurrent() const {
        if (m_window)
            glfwMakeContextCurrent(m_window);
    }

    void ContextManager::swapBuffers() const {
        if (m_window)
            glfwSwapBuffers(m_window);
    }

    void ContextManager::pollEvents() const {
        glfwPollEvents();
    }

    bool ContextManager::shouldClose() const {
        return m_window ? glfwWindowShouldClose(m_window) : true;
    }

} // namespace adapters

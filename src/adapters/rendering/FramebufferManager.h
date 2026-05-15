#pragma once

#include <cstdint>

// NOTE:
// We intentionally avoid including an OpenGL loader header here.
// In this repository, the OpenGL function declarations are provided by the
// Dear ImGui OpenGL3 backend in the translation units that actually issue GL calls.
// Keeping this header free of GL loader includes prevents build-order / include-path
// issues across different build configurations.

using GLuint = unsigned int; // OpenGL object handle (matches the real GLuint type).

namespace adapters {

    // Owns an offscreen framebuffer (color texture + depth/stencil) used to render the 3D viewport.
    // The UI displays the color texture via ImGui::Image.
    class FramebufferManager {
    public:
        FramebufferManager() = default;
        ~FramebufferManager();

        FramebufferManager(const FramebufferManager&) = delete;
        FramebufferManager& operator=(const FramebufferManager&) = delete;

        void initialize(int width, int height);
        void shutdown();

        void resize(int width, int height);

        void bind();
        void unbind();

        GLuint colorTexture() const { return m_colorTex; }
        int width() const { return m_width; }
        int height() const { return m_height; }

    private:
        void recreate();
        void destroy();

        int m_width = 1;
        int m_height = 1;

        GLuint m_fbo = 0;
        GLuint m_colorTex = 0;
        GLuint m_depthRbo = 0;
    };

} // namespace adapters

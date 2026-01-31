
// Provide modern OpenGL enums and function pointers.
#include "adapters/rendering/OpenGLApi.h"

#include "adapters/rendering/FramebufferManager.h"

#include <stdexcept>

namespace adapters {

    FramebufferManager::~FramebufferManager() {
        shutdown();
    }

    void FramebufferManager::initialize(int width, int height) {
        m_width = (width > 1) ? width : 1;
        m_height = (height > 1) ? height : 1;
        recreate();
    }

    void FramebufferManager::shutdown() {
        destroy();
    }

    void FramebufferManager::resize(int width, int height) {
        const int w = (width > 1) ? width : 1;
        const int h = (height > 1) ? height : 1;
        if (w == m_width && h == m_height && m_fbo != 0)
            return;
        m_width = w;
        m_height = h;
        recreate();
    }

    void FramebufferManager::bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
    }

    void FramebufferManager::unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FramebufferManager::recreate() {
        destroy();

        glGenTextures(1, &m_colorTex);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            destroy();
            throw std::runtime_error("OpenGL framebuffer incomplete");
        }
    }

    void FramebufferManager::destroy() {
        if (m_fbo) {
            glDeleteFramebuffers(1, &m_fbo);
            m_fbo = 0;
        }
        if (m_depthRbo) {
            glDeleteRenderbuffers(1, &m_depthRbo);
            m_depthRbo = 0;
        }
        if (m_colorTex) {
            glDeleteTextures(1, &m_colorTex);
            m_colorTex = 0;
        }
    }

} // namespace adapters

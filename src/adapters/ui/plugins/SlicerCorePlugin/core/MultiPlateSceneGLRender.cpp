// MultiPlateSceneGLRender.cpp
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "MultiPlateSceneGLRender.h"
#include "domain/RenderCameraContext.h"
#include <glad/glad.h>
#include <stdexcept>
#include <cstdio>

// Shader is intentionally identical to EditableSceneGLRender's -- same
// lighting model, same shading should apply whether a plate is being
// viewed alone or as part of the grid. Kept as its own copy (rather than
// a shared header) since compileShader/linkProgram are file-local helpers
// there too -- matches the existing pattern instead of introducing a new
// shared-shader indirection for two call sites.
namespace {
    const char* k_vs = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
uniform mat4 uModel, uView, uProj;
out vec3 vNormal;
void main() { vNormal = mat3(uModel) * aNrm; gl_Position = uProj * uView * uModel * vec4(aPos,1.0); }
)GLSL";
    const char* k_fs = R"GLSL(
#version 330 core
in vec3 vNormal;
uniform vec3 uCamPos, uLightDir, uBaseColor;
uniform float uGhostFactor;
out vec4 FragColor;
void main() {
    float diff = max(dot(normalize(vNormal), -normalize(uLightDir)), 0.55);
    FragColor = vec4(uBaseColor * diff, uGhostFactor);
}
)GLSL";

    unsigned int compileShader(unsigned int type, const char* src) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
        int ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            int len = 0; glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0); glGetShaderInfoLog(s, len, nullptr, log.data());
            glDeleteShader(s); throw std::runtime_error("MultiPlateSceneGLRender shader compile failed: " + log);
        }
        return s;
    }
    unsigned int linkProgram(unsigned int vs, unsigned int fs) {
        unsigned int p = glCreateProgram(); glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        int ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            int len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0); glGetProgramInfoLog(p, len, nullptr, log.data());
            glDeleteProgram(p); throw std::runtime_error("MultiPlateSceneGLRender link failed: " + log);
        }
        return p;
    }
}

void MultiPlateSceneGLRender::ensureGl()
{
    if (m_glInitialized) return;
#ifdef _WIN32
    auto loader = [](const char* name) -> void* {
        void* p = (void*)wglGetProcAddress(name);
        if (!p) { static HMODULE gl32 = LoadLibraryA("opengl32.dll"); if (gl32) p = (void*)GetProcAddress(gl32, name); }
        return p; };
    if (!gladLoadGLLoader((GLADloadproc)loader)) { printf("[MultiPlateSceneGLRender] gladLoadGLLoader failed\n"); return; }
#endif
    if (!glad_glCreateProgram) { printf("[MultiPlateSceneGLRender] GL not available\n"); return; }
    try {
        unsigned int vs = compileShader(GL_VERTEX_SHADER, k_vs);
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_fs);
        m_shader = linkProgram(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        m_glInitialized = true;
        printf("[MultiPlateSceneGLRender] GL initialised OK\n");
    }
    catch (const std::exception& e) { printf("[MultiPlateSceneGLRender] GL init failed: %s\n", e.what()); }
}

void MultiPlateSceneGLRender::ensureFramebuffer(uint32_t w, uint32_t h)
{
    if (m_fboWidth == w && m_fboHeight == h && m_fbo) return;
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); glDeleteTextures(1, &m_colorTexture); glDeleteRenderbuffers(1, &m_depthRenderbuffer); m_fbo = m_colorTexture = m_depthRenderbuffer = 0; }
    glGenFramebuffers(1, &m_fbo); glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glGenTextures(1, &m_colorTexture); glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)w, (int)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);
    glGenRenderbuffers(1, &m_depthRenderbuffer); glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (int)w, (int)h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); m_fboWidth = w; m_fboHeight = h;
}

void MultiPlateSceneGLRender::render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx)
{
    ensureGl(); if (!m_glInitialized) return;
    ensureFramebuffer(width, height);
    m_sceneLayout.tick(ctx.deltaSeconds);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, (int)width, (int)height);
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect = (float)width / (float)height;
    auto cameraContext = domain::v1::buildCameraContext(camera, aspect);

    // Ghosting only needs blending while it's actually in effect --
    // matching the old (dead) SceneLayout's anyGhosted-gated GL_BLEND
    // pattern rather than leaving blend state on unconditionally. Alpha
    // itself is uGhostFactor, already wired into the fragment shader
    // above; it just needed blending enabled to actually show through.
    bool ghosting = m_sceneLayout.isGhostAnimating() || m_sceneLayout.anyPlateGhosted();
    if (ghosting)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    m_sceneLayout.render(m_shader, cameraContext);

    if (ghosting) glDisable(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint MultiPlateSceneGLRender::getTexture() const { return m_colorTexture; }

RaycastHit MultiPlateSceneGLRender::raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const CameraState&)
{
    return m_sceneLayout.raycast(rayOrigin, rayDirection);
}
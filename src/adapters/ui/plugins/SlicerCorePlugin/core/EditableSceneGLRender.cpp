// EditableSceneGLRender.cpp
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "EditableSceneGLRender.h"
#include "domain/RenderCameraContext.h"
#include <glad/glad.h>
#include <stdexcept>
#include <cstdio>

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
    // Floor raised from 0.2 -- with the camera-relative "headlamp" light
    // direction (RenderModel::draw()), surfaces facing away from the view
    // direction (e.g. a mostly-horizontal plate seen at a typical orbit
    // angle) were regularly landing at or near the old floor, capping
    // brightness at ~20% of uBaseColor regardless of how bright the color
    // itself was set. This still preserves shading contrast for
    // well-lit surfaces (diff can reach up to 1.0), just without letting
    // anything go this dark.
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
            glDeleteShader(s); throw std::runtime_error("EditableSceneGLRender shader compile failed: " + log);
        }
        return s;
    }
    unsigned int linkProgram(unsigned int vs, unsigned int fs) {
        unsigned int p = glCreateProgram(); glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        int ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            int len = 0; glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0); glGetProgramInfoLog(p, len, nullptr, log.data());
            glDeleteProgram(p); throw std::runtime_error("EditableSceneGLRender link failed: " + log);
        }
        return p;
    }
}

void EditableSceneGLRender::ensureGl()
{
    if (m_glInitialized) return;
#ifdef _WIN32
    auto loader = [](const char* name) -> void* {
        void* p = (void*)wglGetProcAddress(name);
        if (!p) { static HMODULE gl32 = LoadLibraryA("opengl32.dll"); if (gl32) p = (void*)GetProcAddress(gl32, name); }
        return p; };
    if (!gladLoadGLLoader((GLADloadproc)loader)) { printf("[EditableSceneGLRender] gladLoadGLLoader failed\n"); return; }
#endif
    if (!glad_glCreateProgram) { printf("[EditableSceneGLRender] GL not available\n"); return; }
    try {
        unsigned int vs = compileShader(GL_VERTEX_SHADER, k_vs);
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_fs);
        m_shader = linkProgram(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        m_glInitialized = true;
        printf("[EditableSceneGLRender] GL initialised OK\n");
    }
    catch (const std::exception& e) { printf("[EditableSceneGLRender] GL init failed: %s\n", e.what()); }
}

void EditableSceneGLRender::ensureFramebuffer(uint32_t w, uint32_t h)
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

void EditableSceneGLRender::render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx)
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

    m_sceneLayout.render(m_shader, cameraContext);

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint EditableSceneGLRender::getTexture() const { return m_colorTexture; }

RaycastHit EditableSceneGLRender::raycast(const glm::vec3& rayOrigin, const glm::vec3& rayDirection, const CameraState&)
{
    return m_sceneLayout.raycast(rayOrigin, rayDirection);
}
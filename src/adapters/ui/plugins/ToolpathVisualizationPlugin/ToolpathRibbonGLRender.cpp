#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLRender.h"
#include "domain/ToolpathSerialization.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <cmath>
#include <cstdio>

namespace {

    const char* k_ribbonVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main()
{
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

    const char* k_ribbonFS = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

    unsigned int compileShader(unsigned int type, const char* src)
    {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            int len = 0;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0);
            glGetShaderInfoLog(s, len, nullptr, log.data());
            glDeleteShader(s);
            throw std::runtime_error("ToolpathRibbonGLRender shader compile failed: " + log);
        }
        return s;
    }

    unsigned int linkProgram(unsigned int vs, unsigned int fs)
    {
        unsigned int p = glCreateProgram();
        glAttachShader(p, vs); glAttachShader(p, fs);
        glLinkProgram(p);
        int ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            int len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0);
            glGetProgramInfoLog(p, len, nullptr, log.data());
            glDeleteProgram(p);
            throw std::runtime_error("ToolpathRibbonGLRender program link failed: " + log);
        }
        return p;
    }

} // anonymous namespace

ToolpathRibbonGLRender::ToolpathRibbonGLRender() = default;

ToolpathRibbonGLRender::~ToolpathRibbonGLRender()
{
    if (m_shader) glDeleteProgram(m_shader);
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_colorTexture);
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
    }
}

void ToolpathRibbonGLRender::ensureGl()
{
    if (m_glInitialized) return;

#ifdef _WIN32
    auto loader = [](const char* name) -> void* {
        void* p = (void*)wglGetProcAddress(name);
        if (!p) {
            static HMODULE gl32 = LoadLibraryA("opengl32.dll");
            if (gl32) p = (void*)GetProcAddress(gl32, name);
        }
        return p;
        };
    if (!gladLoadGLLoader((GLADloadproc)loader)) {
        printf("[ToolpathRibbonGLRender] gladLoadGLLoader failed\n");
        return;
    }
#endif

    if (!glad_glCreateProgram) {
        printf("[ToolpathRibbonGLRender] GL not available after gladLoadGLLoader\n");
        return;
    }

    try
    {
        unsigned int vs = compileShader(GL_VERTEX_SHADER, k_ribbonVS);
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_ribbonFS);
        m_shader = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        m_glInitialized = true;
        printf("[ToolpathRibbonGLRender] GL initialised OK\n");
    }
    catch (const std::exception& e)
    {
        printf("[ToolpathRibbonGLRender] GL init failed: %s\n", e.what());
    }
}

void ToolpathRibbonGLRender::ensureFramebuffer(uint32_t w, uint32_t h)
{
    if (m_fboWidth == w && m_fboHeight == h && m_fbo) return;

    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_colorTexture);
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
        m_fbo = m_colorTexture = m_depthRenderbuffer = 0;
    }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)w, (int)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    glGenRenderbuffers(1, &m_depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (int)w, (int)h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRenderbuffer);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_fboWidth = w;
    m_fboHeight = h;
}

void ToolpathRibbonGLRender::setToolpath(const domain::v1::Toolpath& toolpath)
{
    ensureGl();
    m_mesh.build(toolpath);
    m_visibleLayer = m_mesh.layerCount() - 1;   // default: show everything

    printf("[ToolpathRibbonGLRender] setToolpath: %d layers\n", m_mesh.layerCount());
}

void ToolpathRibbonGLRender::loadToolpath(const std::string& path)
{
    domain::v1::Toolpath toolpath;
    if (!domain::v1::loadToolpathFromFile(toolpath, path))
    {
        printf("[ToolpathRibbonGLRender] FAILED to load %s\n", path.c_str());
        return;
    }

    setToolpath(toolpath);
    printf("[ToolpathRibbonGLRender] loaded %s\n", path.c_str());
}

void ToolpathRibbonGLRender::setVisibleLayer(int layer)
{
    m_visibleLayer = glm::clamp(layer, 0, m_mesh.layerCount() - 1);
}


 
void ToolpathRibbonGLRender::render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx)
{

    //printf("[ToolpathRibbonGLRender] render() called, w=%u h=%u visibleLayer=%d indexCount=%u\n",
    //    width, height, m_visibleLayer, m_mesh.indexCountForLayer(m_visibleLayer));


    ensureGl();
    if (!m_glInitialized) return;

    ensureFramebuffer(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, (int)width, (int)height);
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect = (float)width / (float)height;
    glm::mat4 proj = glm::perspective(camera.fovYRadians, aspect, camera.nearPlane, camera.farPlane);

    glm::vec3 camPos(
        camera.target.x + camera.distance * cos(camera.pitch) * cos(camera.yaw),
        camera.target.y + camera.distance * sin(camera.pitch),
        camera.target.z + camera.distance * cos(camera.pitch) * sin(camera.yaw));

    glm::mat4 view = glm::lookAt(camPos, camera.target, glm::vec3(0, 1, 0));
    glm::mat4 mvp = proj * view;

    glUseProgram(m_shader);
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(m_mesh.vao());
    uint32_t count = m_mesh.indexCountForLayer(m_visibleLayer);
    if (count > 0)
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)0);
    glBindVertexArray(0);

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint ToolpathRibbonGLRender::getTexture() const
{
    return m_colorTexture;
}

RaycastHit ToolpathRibbonGLRender::raycast(const glm::vec3& origin, const glm::vec3& dir, const CameraState& camera)
{
    return RaycastHit{};   // no picking for this first version
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <imgui.h>
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLRender.h"
#include "domain/ToolpathSerialization.h"
#include "domain/ToolpathBinaryIO.h"
#include "domain/RenderCameraContext.h"
#include "domain/AxisConvention.h"


#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <cmath>
#include <cstdio>
#include <vector>


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
void ToolpathRibbonGLRender::setVisibleLayer(int layer)
{
    setVisibleLayerRange(0, layer);
}


void ToolpathRibbonGLRender::setVisibleLayerRange(int startLayer, int endLayer)
{
    m_layerRangeStart = startLayer;
    m_layerRangeEnd = endLayer;
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



        // Reference grid, Z=0 plane � same shader, drawn with the SAME mvp as
        // the ribbon itself, so any Z misalignment (model floating above/below
        // this plane) is immediately visible, not inferred from printed numbers.
        std::vector<glm::vec3> gridLines;
        const float bedX = 350.0f, bedY = 350.0f;   // TODO: read from config, hardcoded for this test
        const float step = 20.0f;
        for (float x = 0; x <= bedX; x += step)
        {
            gridLines.push_back(glm::vec3(x, 0, 0));
            gridLines.push_back(glm::vec3(x, bedY, 0));
        }
        for (float y = 0; y <= bedY; y += step)
        {
            gridLines.push_back(glm::vec3(0, y, 0));
            gridLines.push_back(glm::vec3(bedX, y, 0));
        }
        m_gridVertexCount = (int)gridLines.size();

        glGenVertexArrays(1, &m_gridVao);
        glGenBuffers(1, &m_gridVbo);
        glBindVertexArray(m_gridVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
        glBufferData(GL_ARRAY_BUFFER, gridLines.size() * sizeof(glm::vec3), gridLines.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);




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
    m_layerRangeStart = 0;
    m_layerRangeEnd = m_mesh.layerCount() - 1;


    if (!toolpath.layers.empty())
    {
        auto& firstLayer = toolpath.layers.front();
        auto& lastLayer = toolpath.layers.back();
        if (!firstLayer.segments.empty() && !lastLayer.segments.empty())
        {
            m_firstLayerFirstSegment = firstLayer.segments.front();
            m_lastLayerLastSegment = lastLayer.segments.back();
        }
    }

    printf("[ToolpathRibbonGLRender] setToolpath: %d layers\n", m_mesh.layerCount());
}

void ToolpathRibbonGLRender::loadToolpath(const std::string& path)
{
    domain::v1::Toolpath toolpath;
    if (!domain::v1::loadToolpathBinary(toolpath, path))
    {
        printf("[ToolpathRibbonGLRender] FAILED to load %s\n", path.c_str());
        return;
    }

    setToolpath(toolpath);
    printf("[ToolpathRibbonGLRender] loaded %s\n", path.c_str());
}


void ToolpathRibbonGLRender::renderUI(ImVec2 topLeft, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // char fpsText[32];
    // snprintf(fpsText, sizeof(fpsText), "%.1f FPS", m_fpsDisplay);

    // ImVec2 textSize = ImGui::CalcTextSize(fpsText);
    // ImVec2 textPos(topLeft.x + size.x - textSize.x - 00.0f, topLeft.y + size.y - textSize.y - 8.0f);

    ///* dl->AddRectFilled(ImVec2(textPos.x - 4, textPos.y - 2), ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2), IM_COL32(0, 0, 0, 130), 3.0f);
    // dl->AddText(textPos, IM_COL32(255, 255, 255, 230), fpsText);*/

    // //topLeft.x = topLeft.x;
    // //topLeft.y = topLeft.y;

    char camerStateText[80];
    snprintf(camerStateText, sizeof(camerStateText), "Yaw %.1f  ,Pitch %.1f ,Distance %.1f ", m_cameraState.yaw, m_cameraState.pitch, m_cameraState.distance);

    ImVec2 camerStateSize = ImGui::CalcTextSize(camerStateText);
    ImVec2 camerStatePos(topLeft.x + size.x - camerStateSize.x - 10.0f, topLeft.y + size.y - camerStateSize.y - 228.0f);

    dl->AddRectFilled(ImVec2(camerStatePos.x - 4, camerStatePos.y - 2), ImVec2(camerStatePos.x + camerStateSize.x + 4, camerStatePos.y + camerStateSize.y + 2), IM_COL32(0, 0, 0, 130), 3.0f);
    dl->AddText(camerStatePos, IM_COL32(255, 255, 255, 230), camerStateText);

    // ... existing FPS block stays here ...


    auto worldPos = [&](const glm::vec3& raw) {
        return glm::vec3(m_lastModelMatrix * glm::vec4(raw, 1.0f));
        };

    glm::vec3 firstRaw = m_firstLayerFirstSegment.start.position;
    glm::vec3 firstWorld = worldPos(firstRaw);
    glm::vec3 lastRaw = m_lastLayerLastSegment.start.position;
    glm::vec3 lastWorld = worldPos(lastRaw);

    char buf[256];

    snprintf(buf, sizeof(buf), "Layer0 raw=(%.2f,%.2f,%.2f) world=(%.2f,%.2f,%.2f)",
        firstRaw.x, firstRaw.y, firstRaw.z, firstWorld.x, firstWorld.y, firstWorld.z);

    ImVec2 textSize2 = ImGui::CalcTextSize(buf);
    ImVec2 textPos2(topLeft.x + size.x - textSize2.x - 10.0f, topLeft.y + size.y - textSize2.y - 148.0f);

    dl->AddText(textPos2, IM_COL32(255, 255, 0, 255), buf);
    textPos2.y += ImGui::GetTextLineHeight() + 2;

    snprintf(buf, sizeof(buf), "LayerN raw=(%.2f,%.2f,%.2f) world=(%.2f,%.2f,%.2f)",
        lastRaw.x, lastRaw.y, lastRaw.z, lastWorld.x, lastWorld.y, lastWorld.z);
    dl->AddText(textPos2, IM_COL32(255, 255, 0, 255), buf);



}

void ToolpathRibbonGLRender::render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx)
{

    //printf("[ToolpathRibbonGLRender] render() called, w=%u h=%u visibleLayer=%d indexCount=%u\n",
    //    width, height, m_visibleLayer, m_mesh.indexCountForLayer(m_visibleLayer));


    ensureGl();
    if (!m_glInitialized) return;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    uint32_t w = (uint32_t)std::max(4.0f, avail.x);
    uint32_t h = (uint32_t)std::max(4.0f, avail.y);
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // --- Delta time ---
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
    dt = std::min(dt, 0.1f); // clamp to avoid huge jumps after stalls/breakpoints

    dt = std::min(dt, 0.1f);

    m_fpsAccumTime += dt;
    m_fpsFrameCount++;
    if (m_fpsAccumTime >= 0.5f)   // refresh twice a second � a raw per-frame number is too jittery to read
    {
        m_fpsDisplay = m_fpsFrameCount / m_fpsAccumTime;
        m_fpsAccumTime = 0.0f;
        m_fpsFrameCount = 0;
    }


    ensureFramebuffer(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, (int)width, (int)height);
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);


    m_cameraState = camera;

    float aspect = (float)width / (float)height;

    // Same fix as DebugComparisonGLRender: this view previously had its
    // own THIRD, separately hand-rolled camera formula (computed right/
    // up/forward manually, then discarded them anyway in favor of
    // glm::lookAt() with a hardcoded world-up of (0,1,0) -- meaning the
    // "Cam Axis" toggle had zero effect here, and it never went through
    // the same fix as the other two views). Get the canonical camera from
    // buildCameraContext() (single source of truth), then remap into this
    // view's native Z-up "bed space" via domain::v1::engineToDomain() --
    // this view's geometry, like DebugComparisonGLRender's, is never
    // axisFix'd, only shifted by bedToSceneTransform below.
    domain::v1::RenderCameraContext engineContext = domain::v1::buildCameraContext(camera, aspect);

    glm::vec3 engRight(engineContext.view[0].x, engineContext.view[1].x, engineContext.view[2].x);
    glm::vec3 engUp(engineContext.view[0].y, engineContext.view[1].y, engineContext.view[2].y);
    glm::vec3 engForward = -glm::vec3(engineContext.view[0].z, engineContext.view[1].z, engineContext.view[2].z);

    glm::vec3 camPos = domain::v1::engineToDomain(engineContext.camPos);
    glm::vec3 right = domain::v1::engineToDomain(engRight);
    glm::vec3 up = domain::v1::engineToDomain(engUp);
    glm::vec3 forward = domain::v1::engineToDomain(engForward);

    glm::mat4 view(
        right.x, up.x, -forward.x, 0.0f,
        right.y, up.y, -forward.y, 0.0f,
        right.z, up.z, -forward.z, 0.0f,
        -glm::dot(right, camPos), -glm::dot(up, camPos), glm::dot(forward, camPos), 1.0f
    );

    int bedSizeX = 350;
    int bedSizeY = 350;
    glm::mat4 bedToSceneTransform = glm::translate(glm::mat4(1.0f),
        glm::vec3(-bedSizeX * 0.5f, -bedSizeY * 0.5f, 0.0f));   // inverse of ImportPhase's corner-offset

    glm::mat4 modelMatrix = bedToSceneTransform;
    glm::mat4 mvp = engineContext.proj * view * modelMatrix;
    m_lastModelMatrix = modelMatrix;

    glUseProgram(m_shader);
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));


    uint32_t offset = 0, count = 0;
    m_mesh.indexRangeForLayers(m_layerRangeStart, m_layerRangeEnd, offset, count);

    glBindVertexArray(m_mesh.vao());
    if (count > 0)
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)(uintptr_t)(offset * sizeof(uint32_t)));
    glBindVertexArray(0);

    // NEW POSITION � grid, still inside the FBO binding
    glUseProgram(m_shader);
    glUniformMatrix4fv(glGetUniformLocation(m_shader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(m_gridVao);
    glDrawArrays(GL_LINES, 0, m_gridVertexCount);
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
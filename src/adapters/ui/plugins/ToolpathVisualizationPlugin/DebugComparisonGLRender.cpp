#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "adapters/ui/plugins/ToolpathVisualizationPlugin/DebugComparisonGLRender.h"
#include "domain/RenderCameraContext.h"
#include "domain/AxisConvention.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>

namespace {

    // --- Model shader: solid colored  with lighting ---
    const char* k_modelSolidVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
uniform mat4 uMVP;
out vec3 vNormal;
void main()
{
    vNormal = aNrm;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

    const char* k_modelSolidFS = R"GLSL(
#version 330 core
in vec3 vNormal;
uniform vec3 uLightDir;   // points FROM camera TOWARD scene � headlamp style, recomputed every frame
uniform vec3 uBaseColor;
out vec4 FragColor;
void main()
{
    float diff = max(dot(normalize(vNormal), -uLightDir), 0.15);   // 0.15 = ambient floor, keeps unlit side visible
    FragColor = vec4(uBaseColor * diff, 1.0);
}
)GLSL";

    // --- Model shader: gradient colored by LOCAL Z (post-recenter) ---
    const char* k_modelVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
out float vLocalZ;
void main()
{
    vLocalZ = aPos.z;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

    const char* k_modelFS = R"GLSL(
#version 330 core
in float vLocalZ;
uniform float uMinZ;
uniform float uMaxZ;
out vec4 FragColor;
void main()
{
    float t = clamp((vLocalZ - uMinZ) / max(uMaxZ - uMinZ, 0.0001), 0.0, 1.0);
    vec3 colorLow = vec3(0.15, 0.25, 0.85);   // blue at bottom
    vec3 colorHigh = vec3(0.95, 0.35, 0.10);  // orange at top
    FragColor = vec4(mix(colorLow, colorHigh, t), 1.0);
}
)GLSL";

    // --- Ribbon shader: same as ToolpathRibbonGLRender's own ---
    const char* k_ribbonVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() { vColor = aColor; gl_Position = uMVP * vec4(aPos, 1.0); }
)GLSL";

    const char* k_ribbonFS = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() { FragColor = vec4(vColor, 1.0); }
)GLSL";

    // --- Line shader: flat color, for bounding-box wireframes ---
    const char* k_lineVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)GLSL";

    const char* k_lineFS = R"GLSL(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() { FragColor = vec4(uColor, 1.0); }
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
            throw std::runtime_error("DebugComparisonGLRender shader compile failed: " + log);
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
            throw std::runtime_error("DebugComparisonGLRender program link failed: " + log);
        }
        return p;
    }

    struct LineVertex { glm::vec3 pos; };

} // anonymous namespace

DebugComparisonGLRender::DebugComparisonGLRender() = default;

DebugComparisonGLRender::~DebugComparisonGLRender()
{
    if (m_modelVao) glDeleteVertexArrays(1, &m_modelVao);
    if (m_modelVbo) glDeleteBuffers(1, &m_modelVbo);
    if (m_modelEbo) glDeleteBuffers(1, &m_modelEbo);
    if (m_modelShader) glDeleteProgram(m_modelShader);
    if (m_lineShader) glDeleteProgram(m_lineShader);
    if (m_modelBoundsVao) glDeleteVertexArrays(1, &m_modelBoundsVao);
    if (m_modelBoundsVbo) glDeleteBuffers(1, &m_modelBoundsVbo);
    if (m_ribbonBoundsVao) glDeleteVertexArrays(1, &m_ribbonBoundsVao);
    if (m_ribbonBoundsVbo) glDeleteBuffers(1, &m_ribbonBoundsVbo);
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_colorTexture);
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
    }
}

void DebugComparisonGLRender::ensureGl()
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
        printf("[DebugComparisonGLRender] gladLoadGLLoader failed\n");
        return;
    }
#endif

    if (!glad_glCreateProgram) {
        printf("[DebugComparisonGLRender] GL not available after gladLoadGLLoader\n");
        return;
    }

    try
    {
        {
            unsigned int vs = compileShader(GL_VERTEX_SHADER, k_modelSolidVS);
            unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_modelSolidFS);
            m_modelSolidShader = linkProgram(vs, fs);
            glDeleteShader(vs); glDeleteShader(fs);

        }

        {

            unsigned int vs = compileShader(GL_VERTEX_SHADER, k_modelVS);
            unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_modelFS);
            m_modelShader = linkProgram(vs, fs);
            glDeleteShader(vs); glDeleteShader(fs);
        }
        {
            unsigned int vs = compileShader(GL_VERTEX_SHADER, k_lineVS);
            unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_lineFS);
            m_lineShader = linkProgram(vs, fs);
            glDeleteShader(vs); glDeleteShader(fs);
        }

        glGenVertexArrays(1, &m_modelVao);
        glGenBuffers(1, &m_modelVbo);
        glGenBuffers(1, &m_modelEbo);

        glGenVertexArrays(1, &m_modelBoundsVao);
        glGenBuffers(1, &m_modelBoundsVbo);
        glGenVertexArrays(1, &m_ribbonBoundsVao);
        glGenBuffers(1, &m_ribbonBoundsVbo);

        m_glInitialized = true;
        printf("[DebugComparisonGLRender] GL initialised OK\n");
    }
    catch (const std::exception& e)
    {
        printf("[DebugComparisonGLRender] GL init failed: %s\n", e.what());
    }
}

void DebugComparisonGLRender::ensureFramebuffer(uint32_t w, uint32_t h)
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
    m_fboWidth = w; m_fboHeight = h;
}

void DebugComparisonGLRender::buildBoundsWireframe(const glm::vec3& mn, const glm::vec3& mx, std::vector<float>& out)
{
    glm::vec3 c[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}
    };
    int edges[12][2] = { {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7} };
    for (auto& e : edges)
    {
        out.push_back(c[e[0]].x); out.push_back(c[e[0]].y); out.push_back(c[e[0]].z);
        out.push_back(c[e[1]].x); out.push_back(c[e[1]].y); out.push_back(c[e[1]].z);
    }
}

void DebugComparisonGLRender::loadModel(domain::v1::ModelCache& cache, const std::string& modelHash)
{
    ensureGl();
    if (!m_glInitialized) return;

    auto model = cache.getModel(modelHash);
    if (!model || !model->mesh)
    {
        printf("[DebugComparisonGLRender] loadModel: no model/mesh for hash %s\n", modelHash.c_str());
        m_hasModel = false;
        return;
    }

    glm::vec3 boundsCenter = (model->mesh->bounds.min + model->mesh->bounds.max) * 0.5f;

    m_modelMinZ = model->mesh->bounds.min.z - boundsCenter.z;
    m_modelMaxZ = model->mesh->bounds.max.z - boundsCenter.z;
    m_modelIndexCount = (uint32_t)model->mesh->indices.size();

    struct ModelVertex { glm::vec3 pos, normal; };   // interleaved layout: position + normal

    std::vector<ModelVertex> vertsWithNormals;
    vertsWithNormals.reserve(model->mesh->vertices.size());
    for (auto& v : model->mesh->vertices)
        vertsWithNormals.push_back({ v.position - boundsCenter, v.normal });

    glBindVertexArray(m_modelVao);

    glBindBuffer(GL_ARRAY_BUFFER, m_modelVbo);
    glBufferData(GL_ARRAY_BUFFER, vertsWithNormals.size() * sizeof(ModelVertex), vertsWithNormals.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(ModelVertex), (void*)offsetof(ModelVertex, normal));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_modelEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->mesh->indices.size() * sizeof(uint32_t), model->mesh->indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    std::vector<float> boundsLines;
    buildBoundsWireframe(model->mesh->bounds.min - boundsCenter, model->mesh->bounds.max - boundsCenter, boundsLines);

    glBindVertexArray(m_modelBoundsVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_modelBoundsVbo);
    glBufferData(GL_ARRAY_BUFFER, boundsLines.size() * sizeof(float), boundsLines.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    m_hasModel = true;

    glm::vec3 modelSize = model->mesh->bounds.max - model->mesh->bounds.min;
    printf("[DebugComparisonGLRender] loadModel: %zu verts, recentered, size=(%.2f,%.2f,%.2f)\n",
        vertsWithNormals.size(), modelSize.x, modelSize.y, modelSize.z);
}
void DebugComparisonGLRender::loadToolpath(const domain::v1::Toolpath& toolpath)
{
    ensureGl();
    if (!m_glInitialized) return;

    glm::vec3 mn(1e30f), mx(-1e30f);
    for (auto& layer : toolpath.layers)
        for (auto& seg : layer.segments)
        {
            mn = glm::min(mn, glm::min(seg.start.position, seg.end.position));
            mx = glm::max(mx, glm::max(seg.start.position, seg.end.position));
        }

    if (toolpath.layers.empty())
    {
        m_hasRibbon = false;
        return;
    }

    glm::vec3 center = (mn + mx) * 0.5f;

    domain::v1::Toolpath recentered = toolpath;
    for (auto& layer : recentered.layers)
        for (auto& seg : layer.segments)
        {
            seg.start.position -= center;
            seg.end.position -= center;
        }

    m_ribbonMesh.build(recentered);

    std::vector<float> boundsLines;
    buildBoundsWireframe(mn - center, mx - center, boundsLines);

    glBindVertexArray(m_ribbonBoundsVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_ribbonBoundsVbo);
    glBufferData(GL_ARRAY_BUFFER, boundsLines.size() * sizeof(float), boundsLines.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    m_hasRibbon = true;
    printf("[DebugComparisonGLRender] loadToolpath: recentered, bounds size=(%.2f,%.2f,%.2f)\n",
        (mx - mn).x, (mx - mn).y, (mx - mn).z);
}

void DebugComparisonGLRender::render(uint32_t width, uint32_t height, const CameraState& camera, const ViewportRenderContext& ctx)
{
    ensureGl();
    if (!m_glInitialized) return;
    ensureFramebuffer(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, (int)width, (int)height);
    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    float aspect = (float)width / (float)height;

    // Fix: get the canonical camera from buildCameraContext() as normal
    // (single source of truth, so this can't silently diverge from the
    // editable scene's camera math again), then remap its basis back into
    // this view's native Z-up space via domain::v1::engineToDomain()
    // (domain/AxisConvention.h -- the one shared, correct inverse of
    // axisFix, instead of a locally hand-typed lambda; that local version
    // got its sign backwards twice in a row before this refactor). That
    // keeps "FRONT" etc. showing the same conceptual face of the model as
    // the editable scene, without ever transforming the actual geometry.
    domain::v1::RenderCameraContext engineContext = domain::v1::buildCameraContext(camera, aspect);

    glm::vec3 engRight(engineContext.view[0].x, engineContext.view[1].x, engineContext.view[2].x);
    glm::vec3 engUp(engineContext.view[0].y, engineContext.view[1].y, engineContext.view[2].y);
    glm::vec3 engForward = -glm::vec3(engineContext.view[0].z, engineContext.view[1].z, engineContext.view[2].z);

    glm::vec3 camPos = domain::v1::engineToDomain(engineContext.camPos);
    glm::vec3 target = domain::v1::engineToDomain(engineContext.target);
    glm::vec3 right = domain::v1::engineToDomain(engRight);
    glm::vec3 up = domain::v1::engineToDomain(engUp);
    glm::vec3 forward = domain::v1::engineToDomain(engForward);   // also used below for uLightDir

    glm::mat4 view(
        right.x, up.x, -forward.x, 0.0f,
        right.y, up.y, -forward.y, 0.0f,
        right.z, up.z, -forward.z, 0.0f,
        -glm::dot(right, camPos), -glm::dot(up, camPos), glm::dot(forward, camPos), 1.0f
    );

    static int s_frameCount = 0;
    if (++s_frameCount % 30 == 0)
    {
        printf("[DebugComparisonGLRender::render] axisMode=%s yaw=%.4f pitch=%.4f distance=%.2f target=%.2f,%.2f,%.2f "
            "camPos=%.2f,%.2f,%.2f forward=%.2f,%.2f,%.2f\n",
            domain::v1::cameraAxisModeName(camera.axisMode), camera.yaw, camera.pitch, camera.distance, target.x, target.y, target.z,
            camPos.x, camPos.y, camPos.z, forward.x, forward.y, forward.z);
    }

    // NO axisFix, NO bedToSceneTransform, NO instance transform -- deliberately
    // identity model matrix. This view compares RAW data shapes only.
    glm::mat4 mvp = engineContext.proj * view;

    if (m_hasModel && m_showModel)
    {

        GLuint activeShader = m_useSolidShader ? m_modelSolidShader : m_modelShader;
        glUseProgram(activeShader);
        glUniformMatrix4fv(glGetUniformLocation(activeShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));

        if (m_useSolidShader)
        {
            glUniform3fv(glGetUniformLocation(activeShader, "uLightDir"), 1, glm::value_ptr(forward));   // camera-following
            glUniform3f(glGetUniformLocation(activeShader, "uBaseColor"), 0.6f, 0.65f, 0.75f);
        }
        else
        {
            glUniform1f(glGetUniformLocation(activeShader, "uMinZ"), m_modelMinZ);
            glUniform1f(glGetUniformLocation(activeShader, "uMaxZ"), m_modelMaxZ);
        }

        glBindVertexArray(m_modelVao);
        glDrawElements(GL_TRIANGLES, m_modelIndexCount, GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }

    if (m_hasRibbon && m_showRibbon)
    {

        static GLuint ribbonShader = 0;
        if (!ribbonShader)
        {
            unsigned int vs = compileShader(GL_VERTEX_SHADER, k_ribbonVS);
            unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_ribbonFS);
            ribbonShader = linkProgram(vs, fs);
            glDeleteShader(vs); glDeleteShader(fs);
        }
        glUseProgram(ribbonShader);
        glUniformMatrix4fv(glGetUniformLocation(ribbonShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glBindVertexArray(m_ribbonMesh.vao());
        uint32_t count = m_ribbonMesh.indexCountForLayer(m_ribbonMesh.layerCount() - 1);
        if (count > 0)
            glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (void*)0);
        glBindVertexArray(0);
    }

    glUseProgram(m_lineShader);
    glUniformMatrix4fv(glGetUniformLocation(m_lineShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));

    if (m_hasModel && m_showModel)
    {
        glUniform3f(glGetUniformLocation(m_lineShader, "uColor"), 0.9f, 0.9f, 0.3f);
        glBindVertexArray(m_modelBoundsVao);
        glDrawArrays(GL_LINES, 0, 24);
        glBindVertexArray(0);
    }
    if (m_hasRibbon && m_showRibbon)
    {
        glUniform3f(glGetUniformLocation(m_lineShader, "uColor"), 0.3f, 0.9f, 0.9f);
        glBindVertexArray(m_ribbonBoundsVao);
        glDrawArrays(GL_LINES, 0, 24);
        glBindVertexArray(0);
    }

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint DebugComparisonGLRender::getTexture() const { return m_colorTexture; }

RaycastHit DebugComparisonGLRender::raycast(const glm::vec3&, const glm::vec3&, const CameraState&) { return RaycastHit{}; }
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "adapters/rendering/CameraGizmoGLRender.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <cstdio>
#include <cmath>

namespace {

    struct GizmoVertex { glm::vec3 pos, nrm; glm::vec2 uv; };

    void pushQuad(std::vector<GizmoVertex>& verts, std::vector<uint32_t>& idx,
        glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 normal)
    {
        uint32_t base = (uint32_t)verts.size();
        verts.push_back({ a, normal, {0,0} });
        verts.push_back({ b, normal, {1,0} });
        verts.push_back({ c, normal, {1,1} });
        verts.push_back({ d, normal, {0,1} });
        idx.insert(idx.end(), { base,base + 1,base + 2, base,base + 2,base + 3 });
    }

    void pushTri(std::vector<GizmoVertex>& verts, std::vector<uint32_t>& idx,
        glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 normal)
    {
        uint32_t base = (uint32_t)verts.size();
        verts.push_back({ a, normal, {0,0} });
        verts.push_back({ b, normal, {0.5f,1} });
        verts.push_back({ c, normal, {1,0} });
        idx.insert(idx.end(), { base,base + 1,base + 2 });
    }

    const char* k_gizmoVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
uniform mat4 uMVP;
uniform mat3 uNormalMat;
out vec3 vNrm;
void main() { vNrm = normalize(uNormalMat * aNrm); gl_Position = uMVP * vec4(aPos, 1.0); }
)GLSL";

    const char* k_gizmoFS = R"GLSL(
#version 330 core
in vec3 vNrm;
uniform vec3 uFaceColor;
uniform vec3 uLightDir;
uniform float uAmbient;
uniform int uHovered;
out vec4 FragColor;
void main()
{
    float diff = max(dot(normalize(vNrm), -uLightDir), 0.0);
    float light = uAmbient + (1.0 - uAmbient) * diff;
    vec3 col = uFaceColor * light;
    if (uHovered == 1) col = mix(col, vec3(1.0), 0.28);
    FragColor = vec4(col, 1.0);
}
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
            throw std::runtime_error("CameraGizmoGLRender shader compile failed: " + log);
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
            throw std::runtime_error("CameraGizmoGLRender program link failed: " + log);
        }
        return p;
    }

} // anonymous namespace

CameraGizmoGLRender::CameraGizmoGLRender() = default;

CameraGizmoGLRender::~CameraGizmoGLRender()
{
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_program) glDeleteProgram(m_program);
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_colorTexture);
        glDeleteRenderbuffers(1, &m_depthRenderbuffer);
    }
}

void CameraGizmoGLRender::ensureGl()
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
        printf("[CameraGizmoGLRender] gladLoadGLLoader failed\n");
        return;
    }
#endif

    if (!glad_glCreateProgram) {
        printf("[CameraGizmoGLRender] GL not available after gladLoadGLLoader\n");
        return;
    }

    try
    {
        unsigned int vs = compileShader(GL_VERTEX_SHADER, k_gizmoVS);
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, k_gizmoFS);
        m_program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        const float S = 1.0f;
        const float C = 0.28f;
        const float Sc = S - C;

        std::vector<GizmoVertex> verts;
        std::vector<uint32_t> idx;
        glm::vec3 n;

        // Main faces
        n = { 0,1,0 };  pushQuad(verts, idx, { -Sc,S,-Sc }, { Sc,S,-Sc }, { Sc,S,Sc }, { -Sc,S,Sc }, n);
        n = { 0,-1,0 }; pushQuad(verts, idx, { -Sc,-S,Sc }, { Sc,-S,Sc }, { Sc,-S,-Sc }, { -Sc,-S,-Sc }, n);
        n = { 0,0,1 };  pushQuad(verts, idx, { -Sc,-Sc,S }, { Sc,-Sc,S }, { Sc,Sc,S }, { -Sc,Sc,S }, n);
        n = { 0,0,-1 }; pushQuad(verts, idx, { Sc,-Sc,-S }, { -Sc,-Sc,-S }, { -Sc,Sc,-S }, { Sc,Sc,-S }, n);
        n = { 1,0,0 };  pushQuad(verts, idx, { S,-Sc,-Sc }, { S,Sc,-Sc }, { S,Sc,Sc }, { S,-Sc,Sc }, n);
        n = { -1,0,0 }; pushQuad(verts, idx, { -S,-Sc,Sc }, { -S,Sc,Sc }, { -S,Sc,-Sc }, { -S,-Sc,-Sc }, n);

        // Top edges
        n = glm::normalize(glm::vec3(0, 1, 1));   pushQuad(verts, idx, { -Sc,Sc,S }, { Sc,Sc,S }, { Sc,S,Sc }, { -Sc,S,Sc }, n);
        n = glm::normalize(glm::vec3(0, 1, -1));  pushQuad(verts, idx, { -Sc,S,-Sc }, { Sc,S,-Sc }, { Sc,Sc,-S }, { -Sc,Sc,-S }, n);
        n = glm::normalize(glm::vec3(1, 1, 0));   pushQuad(verts, idx, { Sc,S,Sc }, { Sc,S,-Sc }, { S,Sc,-Sc }, { S,Sc,Sc }, n);
        n = glm::normalize(glm::vec3(-1, 1, 0));  pushQuad(verts, idx, { -S,Sc,Sc }, { -S,Sc,-Sc }, { -Sc,S,-Sc }, { -Sc,S,Sc }, n);

        // Bottom edges
        n = glm::normalize(glm::vec3(0, -1, 1));  pushQuad(verts, idx, { -Sc,-S,Sc }, { Sc,-S,Sc }, { Sc,-Sc,S }, { -Sc,-Sc,S }, n);
        n = glm::normalize(glm::vec3(0, -1, -1)); pushQuad(verts, idx, { Sc,-Sc,-S }, { -Sc,-Sc,-S }, { -Sc,-S,-Sc }, { Sc,-S,-Sc }, n);
        n = glm::normalize(glm::vec3(1, -1, 0));  pushQuad(verts, idx, { S,-Sc,Sc }, { S,-Sc,-Sc }, { S,-S,-Sc }, { S,-S,Sc }, n);
        n = glm::normalize(glm::vec3(-1, -1, 0)); pushQuad(verts, idx, { -S,-Sc,-Sc }, { -S,-Sc,Sc }, { -S,-S,Sc }, { -S,-S,-Sc }, n);

        // Middle edges
        n = glm::normalize(glm::vec3(1, 0, 1));   pushQuad(verts, idx, { S,-Sc,Sc }, { S,Sc,Sc }, { Sc,Sc,S }, { Sc,-Sc,S }, n);
        n = glm::normalize(glm::vec3(-1, 0, 1));  pushQuad(verts, idx, { -Sc,-Sc,S }, { -Sc,Sc,S }, { -S,Sc,Sc }, { -S,-Sc,Sc }, n);
        n = glm::normalize(glm::vec3(1, 0, -1));  pushQuad(verts, idx, { Sc,-Sc,-S }, { Sc,Sc,-S }, { S,Sc,-Sc }, { S,-Sc,-Sc }, n);
        n = glm::normalize(glm::vec3(-1, 0, -1)); pushQuad(verts, idx, { -S,-Sc,-Sc }, { -S,Sc,-Sc }, { -Sc,Sc,-S }, { -Sc,-Sc,-S }, n);

        // Corner triangles
        n = glm::normalize(glm::vec3(1, 1, 1));    pushTri(verts, idx, { S,Sc,Sc }, { Sc,S,Sc }, { Sc,Sc,S }, n);
        n = glm::normalize(glm::vec3(-1, 1, 1));   pushTri(verts, idx, { -Sc,S,Sc }, { -S,Sc,Sc }, { -Sc,Sc,S }, n);
        n = glm::normalize(glm::vec3(1, 1, -1));   pushTri(verts, idx, { Sc,S,-Sc }, { S,Sc,-Sc }, { Sc,Sc,-S }, n);
        n = glm::normalize(glm::vec3(-1, 1, -1));  pushTri(verts, idx, { -S,Sc,-Sc }, { -Sc,S,-Sc }, { -Sc,Sc,-S }, n);
        n = glm::normalize(glm::vec3(1, -1, 1));   pushTri(verts, idx, { Sc,-Sc,S }, { Sc,-S,Sc }, { S,-Sc,Sc }, n);
        n = glm::normalize(glm::vec3(-1, -1, 1));  pushTri(verts, idx, { -S,-Sc,Sc }, { -Sc,-S,Sc }, { -Sc,-Sc,S }, n);
        n = glm::normalize(glm::vec3(1, -1, -1));  pushTri(verts, idx, { S,-Sc,-Sc }, { Sc,-S,-Sc }, { Sc,-Sc,-S }, n);
        n = glm::normalize(glm::vec3(-1, -1, -1)); pushTri(verts, idx, { -Sc,-Sc,-S }, { -Sc,-S,-Sc }, { -S,-Sc,-Sc }, n);

        m_faces.clear();
        uint32_t off = 0;

        // Snap values converted to radians ONCE, here, for both yaw and
        // pitch — the new global CameraState is radians-only throughout.
        // Original code stored snapYaw already-radians, snapPitch in
        // degrees (see design doc's CameraState units finding) — this
        // resolves that split for new code rather than propagating it.
        auto addFace = [&](uint32_t count, glm::vec3 nrm, glm::vec3 col,
            const char* lbl, float snapYawRad, float snapPitchDeg)
            {
                m_faces.push_back({ off, count, nrm, col, lbl, snapYawRad, glm::radians(snapPitchDeg) });
                off += count;
            };

        auto en = [](float x, float y, float z) { return glm::normalize(glm::vec3(x, y, z)); };

        addFace(6, { 0,1,0 }, { 0.55f,0.65f,0.85f }, "TOP", 0.0f, 89.0f);
        addFace(6, { 0,-1,0 }, { 0.40f,0.45f,0.55f }, "BTM", 0.0f, -89.0f);
        addFace(6, { 0,0,1 }, { 0.45f,0.60f,0.80f }, "FRONT", glm::radians(90.0f), 0.5f);
        addFace(6, { 0,0,-1 }, { 0.35f,0.45f,0.65f }, "BACK", glm::radians(-90.0f), 0.5f);
        addFace(6, { 1,0,0 }, { 0.75f,0.35f,0.35f }, "RIGHT", glm::radians(180.0f), 0.5f);
        addFace(6, { -1,0,0 }, { 0.55f,0.25f,0.25f }, "LEFT", 0.0f, 0.5f);

        glm::vec3 edgeCol = { 0.52f,0.54f,0.60f };
        glm::vec3 cornerCol = { 0.45f,0.47f,0.52f };

        addFace(6, en(0, 1, 1), edgeCol, nullptr, glm::radians(90.f), 45.0f);
        addFace(6, en(0, 1, -1), edgeCol, nullptr, glm::radians(-90.f), 45.0f);
        addFace(6, en(1, 1, 0), edgeCol, nullptr, glm::radians(180.f), 45.0f);
        addFace(6, en(-1, 1, 0), edgeCol, nullptr, 0.0f, 45.0f);
        addFace(6, en(0, -1, 1), edgeCol, nullptr, glm::radians(90.f), -45.0f);
        addFace(6, en(0, -1, -1), edgeCol, nullptr, glm::radians(-90.f), -45.0f);
        addFace(6, en(1, -1, 0), edgeCol, nullptr, glm::radians(180.f), -45.0f);
        addFace(6, en(-1, -1, 0), edgeCol, nullptr, 0.0f, -45.0f);
        addFace(6, en(1, 0, 1), edgeCol, nullptr, glm::radians(135.f), 0.0f);
        addFace(6, en(-1, 0, 1), edgeCol, nullptr, glm::radians(45.f), 0.0f);
        addFace(6, en(1, 0, -1), edgeCol, nullptr, glm::radians(-135.f), 0.0f);
        addFace(6, en(-1, 0, -1), edgeCol, nullptr, glm::radians(-45.f), 0.0f);

        addFace(3, en(1, 1, 1), cornerCol, nullptr, glm::radians(135.f), 35.0f);
        addFace(3, en(-1, 1, 1), cornerCol, nullptr, glm::radians(45.f), 35.0f);
        addFace(3, en(1, 1, -1), cornerCol, nullptr, glm::radians(-135.f), 35.0f);
        addFace(3, en(-1, 1, -1), cornerCol, nullptr, glm::radians(-45.f), 35.0f);
        addFace(3, en(1, -1, 1), cornerCol, nullptr, glm::radians(135.f), -35.0f);
        addFace(3, en(-1, -1, 1), cornerCol, nullptr, glm::radians(45.f), -35.0f);
        addFace(3, en(1, -1, -1), cornerCol, nullptr, glm::radians(-135.f), -35.0f);
        addFace(3, en(-1, -1, -1), cornerCol, nullptr, glm::radians(-45.f), -35.0f);

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);
        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GizmoVertex), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)offsetof(GizmoVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)offsetof(GizmoVertex, nrm));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GizmoVertex), (void*)offsetof(GizmoVertex, uv));

        glBindVertexArray(0);

        m_glInitialized = true;
        printf("[CameraGizmoGLRender] GL initialised OK, %d faces\n", (int)m_faces.size());
    }
    catch (const std::exception& e)
    {
        printf("[CameraGizmoGLRender] GL init failed: %s\n", e.what());
    }
}

void CameraGizmoGLRender::ensureFramebuffer(uint32_t w, uint32_t h)
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

void CameraGizmoGLRender::render(uint32_t width, uint32_t height, const CameraState& camera)
{
    ensureGl();
    if (!m_glInitialized) return;

    ensureFramebuffer(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, (int)width, (int)height);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    float aspect = (float)width / (float)height;
    glm::mat4 proj = glm::perspective(glm::radians(28.0f), aspect, 0.1f, 20.0f);

    // Orient the gizmo to match the main camera's yaw/pitch — both
    // already radians in the new global CameraState, used directly,
    // no conversion needed (unlike the original code's pitch).
    float cy = std::cos(camera.yaw), sy = std::sin(camera.yaw);
    float cp = std::cos(camera.pitch), sp = std::sin(camera.pitch);

    float dist = 8.5f;
    glm::vec3 camPos(dist * cy * cp, dist * sp, dist * sy * cp);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 vp = proj * view;

    glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1.5f, -1));

    glUseProgram(m_program);
    glUniform3fv(glGetUniformLocation(m_program, "uLightDir"), 1, glm::value_ptr(lightDir));
    glUniform1f(glGetUniformLocation(m_program, "uAmbient"), 0.35f);

    glBindVertexArray(m_vao);

    for (size_t i = 0; i < m_faces.size(); ++i)
    {
        const GizmoFace& f = m_faces[i];
        glm::mat4 mvp = vp;
        glm::mat3 nmat(1.0f);

        glUniformMatrix4fv(glGetUniformLocation(m_program, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix3fv(glGetUniformLocation(m_program, "uNormalMat"), 1, GL_FALSE, glm::value_ptr(nmat));
        glUniform3fv(glGetUniformLocation(m_program, "uFaceColor"), 1, glm::value_ptr(f.color));
        glUniform1i(glGetUniformLocation(m_program, "uHovered"), (int)(m_hoveredFace == (int)i));

        glDrawElements(GL_TRIANGLES, f.idxCount, GL_UNSIGNED_INT, (void*)(uintptr_t)(f.idxOffset * sizeof(uint32_t)));
    }

    m_lastView = view;
    m_lastProj = proj;
    m_lastCamFwd = glm::normalize(-camPos);

    glBindVertexArray(0);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint CameraGizmoGLRender::getTexture() const
{
    return m_colorTexture;
}

glm::vec2 CameraGizmoGLRender::project2D(const glm::vec3& worldPos, uint32_t gizmoW, uint32_t gizmoH) const
{
    glm::mat4 vp = m_lastProj * m_lastView;
    glm::vec4 clip = vp * glm::vec4(worldPos, 1.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return glm::vec2(
        (ndc.x * 0.5f + 0.5f) * gizmoW,
        (1.0f - (ndc.y * 0.5f + 0.5f)) * gizmoH);
}

bool CameraGizmoGLRender::hitTestFace(float localX, float localY, uint32_t gizmoW, uint32_t gizmoH, float& outYaw, float& outPitch) const
{
    float nx = (localX / (float)gizmoW) * 2.0f - 1.0f;
    float ny = 1.0f - (localY / (float)gizmoH) * 2.0f;

    glm::vec4 rayClip(nx, ny, -1, 1);
    glm::vec4 rayEye = glm::inverse(m_lastProj) * rayClip;
    rayEye = { rayEye.x, rayEye.y, -1, 0 };
    glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(m_lastView) * rayEye));

    float bestDot = -1.0f;
    int bestFace = -1;
    for (size_t i = 0; i < m_faces.size(); ++i)
    {
        const auto& f = m_faces[i];
        if (glm::dot(f.normal, m_lastCamFwd) < -0.05f)   // camera-facing only
        {
            float d = glm::dot(f.normal, -rayDir);
            if (d > bestDot) { bestDot = d; bestFace = (int)i; }
        }
    }

    if (bestFace < 0) return false;

    outYaw = m_faces[bestFace].snapYaw;
    outPitch = m_faces[bestFace].snapPitch;
    return true;
}
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include "StlPreviewRenderer.h"

#include "FolderScanner.h" // for path utilities — remove if not needed

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include "domain\dataContext.h"

  

namespace slicer {

    // -----------------------------------------------------------------------
    // Shaders
    // -----------------------------------------------------------------------

    static const char* k_meshVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCamPos;

out vec3 vPosW;
out vec3 vNrmW;

void main()
{
    vec4 posW  = uModel * vec4(aPos, 1.0);
    vPosW      = posW.xyz;
    vNrmW      = normalize(mat3(transpose(inverse(uModel))) * aNrm);
    gl_Position = uProj * uView * posW;
}
)GLSL";

    static const char* k_meshFS = R"GLSL(
#version 330 core
in vec3 vPosW;
in vec3 vNrmW;

uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uBaseColor;

out vec4 FragColor;

void main()
{
    vec3 N = normalize(vNrmW);
    vec3 L = normalize(-uLightDir);
    vec3 V = normalize(uCamPos - vPosW);
    vec3 H = normalize(L + V);

    float diff    = max(dot(N, L), 0.0);
    float spec    = pow(max(dot(N, H), 0.0), 32.0) * 0.4;
    float ambient = 0.15;

    vec3 color = uBaseColor * (ambient + diff) + vec3(spec);

    // Subtle back-face fill so all sides are visible
    if (!gl_FrontFacing)
        color = uBaseColor * 0.25;

    // Simple tonemap + gamma
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}
)GLSL";

    static const char* k_gridVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uView;
uniform mat4 uProj;
void main() { gl_Position = uProj * uView * vec4(aPos, 1.0); }
)GLSL";

    static const char* k_gridFS = R"GLSL(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)GLSL";


    

    // -----------------------------------------------------------------------
    // StlPreviewRenderer
    // -----------------------------------------------------------------------

    StlPreviewRenderer::StlPreviewRenderer() = default;

    StlPreviewRenderer::~StlPreviewRenderer()
    {
        destroyGl();
    }

    bool StlPreviewRenderer::initialize(ModelCache& cache)
    {
        return true;
    }

    bool StlPreviewRenderer::loadMesh(std::shared_ptr<domain::v1::Mesh> mesh)
    {
        if (!ensureGl()) return false;
        if (mesh->vertices.empty() || mesh->indices.empty()) return false;

        // Build flat float buffer from indexed mesh
        std::vector<float> verts;
        verts.reserve(mesh->indices.size() * 6);
        for (uint32_t idx : mesh->indices)
        {
            const Vertex& v = mesh->vertices[idx];
            verts.push_back(v.position.x); verts.push_back(v.position.y); verts.push_back(v.position.z);
            verts.push_back(v.normal.x);   verts.push_back(v.normal.y);   verts.push_back(v.normal.z);
        }

        m_meshCenter = mesh->center;
        m_meshRadius = mesh->radius;
        m_distance = mesh->cameradistance;
        m_meshIndexCount = (int)mesh->indices.size();

        // Upload to GPU
        if (!m_meshVao) glGenVertexArrays(1, &m_meshVao);
        if (!m_meshVbo) glGenBuffers(1, &m_meshVbo);

        glBindVertexArray(m_meshVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_meshVbo);
        glBufferData(GL_ARRAY_BUFFER,
            (GLsizeiptr)(verts.size() * sizeof(float)),
            verts.data(), GL_STATIC_DRAW);

        const int stride = 6 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glBindVertexArray(0);

        m_meshLoaded = true;
        createGrid(m_meshRadius * 2.5f, 10);
        return true;
    }
    // -----------------------------------------------------------------------
    

    // -----------------------------------------------------------------------
    void StlPreviewRenderer::clear()
    {
        m_meshLoaded = false;
        m_meshIndexCount = 0;
        m_meshCenter = { 0,0,0 };
        m_meshRadius = 1.0f;
        m_distance = 3.0f;
    }

    // -----------------------------------------------------------------------
    void StlPreviewRenderer::tick(float dtSeconds)
    {
        if (!paused)
            m_yaw += autoRotateSpeed * dtSeconds;
    }

    // -----------------------------------------------------------------------
    void StlPreviewRenderer::render(uint32_t width, uint32_t height)
    {
            if (!ensureGl()) return;

            width = std::max(1u, width);
            height = std::max(1u, height);

            ensureFbo(width, height);
            updateCamera(width, height);

            glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
            glViewport(0, 0, (int)width, (int)height);

            glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);

            // Draw grid first
            renderGrid();

            // Draw mesh on top
            if (m_meshLoaded)
                renderMesh();

            glDisable(GL_DEPTH_TEST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // -----------------------------------------------------------------------
    void* StlPreviewRenderer::getTexture() const
    {
        return (void*)(intptr_t)m_fboColor;
    }

    // -----------------------------------------------------------------------
    // Private
    // -----------------------------------------------------------------------

    bool StlPreviewRenderer::ensureGl()
    {
        if (m_glReady) return true;

        // Do NOT call gladLoadGL here — the host has already loaded GLAD.
        // Since glad is a static lib linked into both host and plugin, the
        // plugin has its own function pointer table. We need to copy the
        // host's loaded pointers by calling gladLoadGLLoader again with
        // the same proc address function.
        //
        // Use wglGetProcAddress + GetProcAddress as the loader — same as host.
#ifdef _WIN32
        auto loader = [](const char* name) -> void*
            {
                void* p = (void*)wglGetProcAddress(name);
                if (!p)
                {
                    static HMODULE gl32 = LoadLibraryA("opengl32.dll");
                    if (gl32) p = (void*)GetProcAddress(gl32, name);
                }
                return p;
            };

        if (!gladLoadGLLoader((GLADloadproc)loader))
        {
            printf("[StlPreview] gladLoadGLLoader failed\n");
            return false;
        }
#endif

        if (!glad_glCreateProgram)
        {
            printf("[StlPreview] GL not available after gladLoadGLLoader\n");
            return false;
        }

        try {
            createShaders();
            createGrid(2.5f, 10);
            m_glReady = true;
            printf("[StlPreview] GL initialised OK\n");
        }
        catch (const std::exception& e) {
            printf("[StlPreview] GL init failed: %s\n", e.what());
            return false;
        }
        return true;
    }
    void StlPreviewRenderer::createShaders()
    {
        unsigned int vs, fs;

        // Mesh shader
        vs = compileShader(GL_VERTEX_SHADER, k_meshVS);
        fs = compileShader(GL_FRAGMENT_SHADER, k_meshFS);
        m_meshProgram = linkProgram(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);

        // Grid shader
        vs = compileShader(GL_VERTEX_SHADER, k_gridVS);
        fs = compileShader(GL_FRAGMENT_SHADER, k_gridFS);
        m_gridProgram = linkProgram(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
    }

    void StlPreviewRenderer::createGrid(float size, int divisions)
    {
        // Grid sits at Y=0 (world floor). Mesh is centred and rotated to sit on it.
        std::vector<glm::vec3> lines;
        float step = (size * 2.0f) / (float)divisions;

        for (int i = 0; i <= divisions; ++i)
        {
            float t = -size + step * i;
            lines.push_back({ t,     0.0f, -size });
            lines.push_back({ t,     0.0f,  size });
            lines.push_back({ -size, 0.0f,  t });
            lines.push_back({ size, 0.0f,  t });
        }
        m_gridLineCount = (int)lines.size();

        if (!m_gridVao) glGenVertexArrays(1, &m_gridVao);
        if (!m_gridVbo) glGenBuffers(1, &m_gridVbo);

        glBindVertexArray(m_gridVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_gridVbo);
        glBufferData(GL_ARRAY_BUFFER,
            (GLsizeiptr)(lines.size() * sizeof(glm::vec3)),
            lines.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);
    }
    void StlPreviewRenderer::ensureFbo(uint32_t w, uint32_t h)
    {
        if (m_fboWidth == w && m_fboHeight == h && m_fbo) return;

        if (m_fbo)
        {
            glDeleteFramebuffers(1, &m_fbo);
            glDeleteTextures(1, &m_fboColor);
            glDeleteRenderbuffers(1, &m_fboDepth);
            m_fbo = m_fboColor = m_fboDepth = 0;
        }

        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

        glGenTextures(1, &m_fboColor);
        glBindTexture(GL_TEXTURE_2D, m_fboColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)w, (int)h,
            0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, m_fboColor, 0);

        glGenRenderbuffers(1, &m_fboDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_fboDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (int)w, (int)h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, m_fboDepth);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_fboWidth = w;
        m_fboHeight = h;
    }

    void StlPreviewRenderer::updateCamera(uint32_t w, uint32_t h)
    {
        // Camera orbits around world origin at height m_meshRadius * 0.3
        // so it looks slightly above the bedplate centre.
        // After the model transform the mesh sits on Y=0 centred at origin.
        glm::vec3 target(0.0f, m_meshRadius * 0.3f, 0.0f);

        glm::vec3 dir{
            std::cos(m_pitch) * std::cos(m_yaw),
            std::sin(m_pitch),
            std::cos(m_pitch) * std::sin(m_yaw)
        };

        glm::vec3 camPos = target + dir * m_distance;

        m_view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));
        m_camPos = camPos;

        float aspect = (h > 0) ? (float)w / (float)h : 1.0f;

        // Generous near/far — no clipping during full orbit
        float near_ = 0.01f;
        float far_ = m_distance * 2.0f + m_meshRadius * 6.0f;
        m_proj = glm::perspective(glm::radians(45.0f), aspect, near_, far_);
    }

    void StlPreviewRenderer::renderMesh()
    {
        if (!m_meshProgram || !m_meshVao || m_meshIndexCount == 0) return;

        // Model transform:
        // STL files are Z-up. We want Y-up so the part lies flat on the grid.
        //
        // Steps (applied right to left):
        //   1. Translate so mesh centre is at origin
        //   2. Rotate -90 deg around X  (Z-up -> Y-up)
        //   3. Translate up so the bottom of the mesh sits on Y=0 (the grid)
        //
        // After step 2 the mesh occupies [-r, +r] on Y.
        // Shifting up by +r puts the bottom face on Y=0.

        glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -m_meshCenter);
        glm::mat4 rotate = glm::rotate(glm::mat4(1.0f),
            glm::radians(-90.0f),
            glm::vec3(1.0f, 0.0f, 0.0f));
        glm::mat4 sitOnBed = glm::translate(glm::mat4(1.0f),
            glm::vec3(0.0f, m_meshRadius * 0.5f, 0.0f));

        glm::mat4 model = sitOnBed * rotate * toOrigin;

        glUseProgram(m_meshProgram);
        glUniformMatrix4fv(glGetUniformLocation(m_meshProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(m_meshProgram, "uView"), 1, GL_FALSE, glm::value_ptr(m_view));
        glUniformMatrix4fv(glGetUniformLocation(m_meshProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(m_proj));
        glUniform3f(glGetUniformLocation(m_meshProgram, "uCamPos"),
            m_camPos.x, m_camPos.y, m_camPos.z);
        glUniform3f(glGetUniformLocation(m_meshProgram, "uLightDir"),
            -0.4f, -0.8f, -0.4f);
        glUniform3f(glGetUniformLocation(m_meshProgram, "uBaseColor"),
            0.65f, 0.35f, 0.85f);

        glBindVertexArray(m_meshVao);
        glDrawArrays(GL_TRIANGLES, 0, m_meshIndexCount);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    void StlPreviewRenderer::renderGrid()
    {
        if (!m_gridProgram || !m_gridVao || m_gridLineCount == 0) return;

        glUseProgram(m_gridProgram);
        glUniformMatrix4fv(glGetUniformLocation(m_gridProgram, "uView"), 1, GL_FALSE, glm::value_ptr(m_view));
        glUniformMatrix4fv(glGetUniformLocation(m_gridProgram, "uProj"), 1, GL_FALSE, glm::value_ptr(m_proj));
        glUniform4f(glGetUniformLocation(m_gridProgram, "uColor"),
            0.30f, 0.30f, 0.35f, 1.0f);

        glBindVertexArray(m_gridVao);
        glDrawArrays(GL_LINES, 0, m_gridLineCount);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    void StlPreviewRenderer::destroyGl()
    {
        if (!m_glReady) return;

        if (m_meshProgram) { glDeleteProgram(m_meshProgram); m_meshProgram = 0; }
        if (m_meshVao) { glDeleteVertexArrays(1, &m_meshVao); m_meshVao = 0; }
        if (m_meshVbo) { glDeleteBuffers(1, &m_meshVbo); m_meshVbo = 0; }

        if (m_gridProgram) { glDeleteProgram(m_gridProgram); m_gridProgram = 0; }
        if (m_gridVao) { glDeleteVertexArrays(1, &m_gridVao); m_gridVao = 0; }
        if (m_gridVbo) { glDeleteBuffers(1, &m_gridVbo); m_gridVbo = 0; }

        if (m_fbo)
        {
            glDeleteFramebuffers(1, &m_fbo);
            glDeleteTextures(1, &m_fboColor);
            glDeleteRenderbuffers(1, &m_fboDepth);
            m_fbo = m_fboColor = m_fboDepth = 0;
        }

        m_glReady = false;
        m_meshLoaded = false;
    }

    unsigned int StlPreviewRenderer::compileShader(unsigned int type, const char* src)
    {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            int len = 0;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0);
            glGetShaderInfoLog(s, len, nullptr, log.data());
            glDeleteShader(s);
            throw std::runtime_error("Shader compile failed: " + log);
        }
        return s;
    }

    unsigned int StlPreviewRenderer::linkProgram(unsigned int vs, unsigned int fs)
    {
        unsigned int p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        int ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            int len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, 0);
            glGetProgramInfoLog(p, len, nullptr, log.data());
            glDeleteProgram(p);
            throw std::runtime_error("Program link failed: " + log);
        }
        return p;
    }

} // namespace slicer
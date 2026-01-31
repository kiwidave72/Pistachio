#include "adapters/rendering/GlCubeViewRenderer.h"

// OpenGL loader: this repo already builds and links glad, so use it here.
// (Keeping the loader include in the .cpp avoids Windows include-order footguns.)
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <string>
#include <algorithm>

namespace adapters {

    namespace {
        bool EnsureGlContextAndGlad()
        {
            // Must have a current context for gladLoadGLLoader to work.
            if (glfwGetCurrentContext() == nullptr) {
                std::printf("[GlCubeViewRenderer][WARN] No current GLFW context; skipping GL init this call.\n");
                return false;
            }

            // If we already have function pointers, we're good.
            if (glad_glGetString && glad_glCreateShader)
                return true;

            // Load OpenGL entry points.
            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                std::printf("[GlCubeViewRenderer][ERR] gladLoadGLLoader failed (no GL function pointers).\n");
                return false;
            }
            return true;
        }

        bool HasBasicGl()
        {
            return glad_glCreateProgram && glad_glGenVertexArrays && glad_glBindVertexArray;
        }
    }

    static GLuint compileShader(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log;
            log.resize(len > 0 ? (size_t)len : 0);
            if (len > 0) glGetShaderInfoLog(s, len, nullptr, log.data());
            glDeleteShader(s);
            throw std::runtime_error("OpenGL shader compile failed: " + log);
        }
        return s;
    }

    static GLuint linkProgram(GLuint vs, GLuint fs) {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log;
            log.resize(len > 0 ? (size_t)len : 0);
            if (len > 0) glGetProgramInfoLog(p, len, nullptr, log.data());
            glDeleteProgram(p);
            throw std::runtime_error("OpenGL program link failed: " + log);
        }
        return p;
    }

    GlCubeViewRenderer::GlCubeViewRenderer() = default;
    GlCubeViewRenderer::~GlCubeViewRenderer()
    {
        shutdown();
    }

    bool GlCubeViewRenderer::initialize() {
        // initialize() may be called before the GLFW context exists/is current.
        // Defer to first render() if GL isn't ready yet.
        
        // ===== BUG FIX: Fixed the logic flow =====
        if (!EnsureGlContextAndGlad()) {
            std::printf("[GlCubeViewRenderer][WARN] No current GLFW context yet; deferring GL init until first render.\n");
            return true;  // Return success - resources will be created on first render()
        }

        // GL context is ready, create resources now
        try {
            createResources();
            updateDerivedCamera();
            return true;
        } catch (const std::exception& e) {
            std::printf("[GlCubeViewRenderer][ERR] Exception during initialize(): %s\n", e.what());
            return false;
        } catch (...) {
            std::printf("[GlCubeViewRenderer][ERR] Unknown exception during initialize()\n");
            return false;
        }
    }

    void GlCubeViewRenderer::shutdown() {
        // If shutdown happens after GLFW teardown, GL calls may crash.
        if (glfwGetCurrentContext() != nullptr) {
            m_framebuffer.shutdown();
            destroyResources();
        }
    }

    void GlCubeViewRenderer::createResources() {
        // Shaded cube (positions + normals + UV + tangent).
        // Unit cube centered at origin, scaled in model matrix.
        struct V {
            float px, py, pz;
            float nx, ny, nz;
            float u, v;
            float tx, ty, tz; // tangent (world after model transform, but we build in object-space)
        };

        // 24 unique verts (4 per face) for correct hard normals.
        // UVs are per-face, 0..1. Tangent points along +U on each face.
        const V verts[] = {
            // +X (u=z, v=y)
            { 1, -1, -1,  1,0,0,   0,0,   0,0,1 },
            { 1,  1, -1,  1,0,0,   0,1,   0,0,1 },
            { 1,  1,  1,  1,0,0,   1,1,   0,0,1 },
            { 1, -1,  1,  1,0,0,   1,0,   0,0,1 },

            // -X (u=-z, v=y)
            { -1, -1,  1, -1,0,0,  0,0,   0,0,-1 },
            { -1,  1,  1, -1,0,0,  0,1,   0,0,-1 },
            { -1,  1, -1, -1,0,0,  1,1,   0,0,-1 },
            { -1, -1, -1, -1,0,0,  1,0,   0,0,-1 },

            // +Y (u=x, v=-z)
            { -1, 1, -1, 0,1,0,    0,0,   1,0,0 },
            { -1, 1,  1, 0,1,0,    0,1,   1,0,0 },
            {  1, 1,  1, 0,1,0,    1,1,   1,0,0 },
            {  1, 1, -1, 0,1,0,    1,0,   1,0,0 },

            // -Y (u=x, v=z)
            { -1,-1,  1, 0,-1,0,   0,0,   1,0,0 },
            { -1,-1, -1, 0,-1,0,   0,1,   1,0,0 },
            {  1,-1, -1, 0,-1,0,   1,1,   1,0,0 },
            {  1,-1,  1, 0,-1,0,   1,0,   1,0,0 },

            // +Z (u=-x, v=y)
            {  1,-1,1, 0,0,1,      0,0,   -1,0,0 },
            {  1, 1,1, 0,0,1,      0,1,   -1,0,0 },
            { -1, 1,1, 0,0,1,      1,1,   -1,0,0 },
            { -1,-1,1, 0,0,1,      1,0,   -1,0,0 },

            // -Z (u=x, v=y)
            { -1,-1,-1, 0,0,-1,    0,0,   1,0,0 },
            { -1, 1,-1, 0,0,-1,    0,1,   1,0,0 },
            {  1, 1,-1, 0,0,-1,    1,1,   1,0,0 },
            {  1,-1,-1, 0,0,-1,    1,0,   1,0,0 },
        };

        const uint16_t idx[] = {
            0,1,2, 0,2,3,          // +X
            4,5,6, 4,6,7,          // -X
            8,9,10, 8,10,11,       // +Y
            12,13,14, 12,14,15,    // -Y
            16,17,18, 16,18,19,    // +Z
            20,21,22, 20,22,23     // -Z
        };

        const char* vsSrc = R"GLSL(
            #version 330 core
            layout(location=0) in vec3 aPos;
            layout(location=1) in vec3 aNrm;
            layout(location=2) in vec2 aUV;
            layout(location=3) in vec3 aTan;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProj;

            out vec3 vPosW;
            out vec3 vNrmW;
            out vec2 vUV;
            out mat3 vTBN;

            void main() {
                vec4 posW = uModel * vec4(aPos, 1.0);
                vPosW = posW.xyz;
                mat3 nrmMat = mat3(transpose(inverse(uModel)));
                vec3 N = normalize(nrmMat * aNrm);
                vec3 T = normalize(mat3(uModel) * aTan);
                // Re-orthonormalize T relative to N
                T = normalize(T - N * dot(N, T));
                vec3 B = normalize(cross(N, T));

                vNrmW = N;
                vUV = aUV;
                vTBN = mat3(T, B, N);
                gl_Position = uProj * uView * posW;
            }
        )GLSL";

        const char* fsSrc = R"GLSL(
            #version 330 core
            in vec3 vPosW;
            in vec3 vNrmW;
            in vec2 vUV;
            in mat3 vTBN;

            uniform vec3 uCamPos;
            uniform vec3 uLightDir;
            uniform vec3 uBaseColor;

            uniform float uBumpStrength;
            uniform float uMetalness;
            uniform float uRoughness;

            out vec4 FragColor;

            // Trivial bump function: sin waves in UV space.
            vec3 sampleProcBump(vec2 uv, float strength) {
                float scale = 8.0;
                float bx = sin(uv.x * scale) * 0.5;
                float by = sin(uv.y * scale) * 0.5;
                vec3 bumpOffset = vec3(bx, by, 1.0);
                return normalize(mix(vec3(0, 0, 1), bumpOffset, strength));
            }

            // Schlick Fresnel approximation
            vec3 fresnelSchlick(float cosTheta, vec3 F0) {
                return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
            }

            void main() {
                // Procedural bump in tangent space
                vec3 bumpTS = sampleProcBump(vUV, uBumpStrength);
                vec3 N = normalize(vTBN * bumpTS);

                vec3 L = normalize(-uLightDir);
                vec3 V = normalize(uCamPos - vPosW);
                vec3 H = normalize(L + V);

                float NdotL = max(dot(N, L), 0.0);
                float NdotV = max(dot(N, V), 0.0);
                float NdotH = max(dot(N, H), 0.0);

                // Metalness-dependent F0
                vec3 F0 = mix(vec3(0.04), uBaseColor, uMetalness);

                // Simplified Fresnel + roughness specular
                vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
                float alpha = uRoughness * uRoughness;
                float D = (NdotH * NdotH) * (alpha * alpha - 1.0) + 1.0;
                D = (alpha * alpha) / max(3.14159 * D * D, 0.0001);

                vec3 specular = F * D;

                // Lambertian diffuse scaled by (1-metalness)
                vec3 diffuse = (1.0 - F) * (1.0 - uMetalness) * uBaseColor / 3.14159;

                vec3 directLight = (diffuse + specular) * NdotL;

                // Ambient fill (prevents fully black faces)
                vec3 ambient = uBaseColor * 0.15;

                vec3 color = ambient + directLight;

                // Simple tonemap
                color = color / (color + vec3(1.0));

                // Gamma
                color = pow(color, vec3(1.0 / 2.2));

                FragColor = vec4(color, 1.0);
            }
        )GLSL";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
        m_program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        // VAO + VBO + IBO
        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        GLuint ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        // Layout: pos(3), nrm(3), uv(2), tan(3) => 11 floats per vertex
        int stride = 11 * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));

        glBindVertexArray(0);

        m_vbo = vbo;
        m_ebo = ebo;
    }

    void GlCubeViewRenderer::destroyResources() {
        if (m_program) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        if (m_vbo) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        if (m_ebo) {
            glDeleteBuffers(1, &m_ebo);
            m_ebo = 0;
        }
    }

    void GlCubeViewRenderer::updateDerivedCamera() {
        // Compute camera position from yaw/pitch/distance
        // yaw=0 -> look from +X; pitch=0 -> on XZ plane

        glm::vec3 dir{
            std::cos(m_pitch) * std::cos(m_yaw),
            std::sin(m_pitch),
            std::cos(m_pitch) * std::sin(m_yaw)
        };

        m_camera.target = glm::vec3(0.0f);
        m_camera.position = m_camera.target + (-dir) * m_distance;
        m_camera.up = glm::vec3(0, 1, 0);

        m_view = glm::lookAt(m_camera.position, m_camera.target, m_camera.up);

        // Dynamic clip planes (helps avoid precision issues and "clipping" when zooming)
        // Keep near not too small, but also ensure it stays in front of the object.
        const float safety = 2.5f;
        m_camera.nearPlane = std::max(0.05f, m_distance - m_objectRadius * safety);
        m_camera.farPlane  = std::max(m_camera.nearPlane + 10.0f, m_distance + m_objectRadius * safety);

        float aspect = (m_viewHeight > 0) ? (float)m_viewWidth / (float)m_viewHeight : 1.0f;
        if (m_camera.orthographic) {
            float s = std::max(0.001f, m_camera.orthoScale);
            m_proj = glm::ortho(-s * aspect, s * aspect, -s, s, m_camera.nearPlane, m_camera.farPlane);
        } else {
            m_proj = glm::perspective(m_camera.fovYRadians, aspect, m_camera.nearPlane, m_camera.farPlane);
        }
    }

    void GlCubeViewRenderer::render(GLFWwindow* window) {
        // Ensure we have a current context before any GL calls.
        if (window)
            glfwMakeContextCurrent(window);

        // Lazy init: if initialize() ran before a context existed, build GL resources now.
        if (!m_program || !m_vao) {
            if (!EnsureGlContextAndGlad())
                return;
            createResources();
            updateDerivedCamera();
        }

        if (window) {
            // Draw into the host window backbuffer.
            int fbw = 1, fbh = 1;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            if (fbw <= 0 || fbh <= 0) return;
            if ((uint32_t)fbw != m_viewWidth || (uint32_t)fbh != m_viewHeight) {
                m_viewWidth = (uint32_t)fbw;
                m_viewHeight = (uint32_t)fbh;
                updateDerivedCamera();
            }
            glViewport(0, 0, (int)m_viewWidth, (int)m_viewHeight);
        } else {
            // Offscreen path assumes viewport already set by caller.
            if (m_viewWidth == 0 || m_viewHeight == 0) return;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE); // keep simple for now; can enable later

        glEnable(GL_MULTISAMPLE);

        // Background (neutral dark)
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Cube model matrix.
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(m_modelScale));

        glUseProgram(m_program);

        glUniformMatrix4fv(glGetUniformLocation(m_program, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(m_program, "uView"),  1, GL_FALSE, glm::value_ptr(m_view));
        glUniformMatrix4fv(glGetUniformLocation(m_program, "uProj"),  1, GL_FALSE, glm::value_ptr(m_proj));

        glUniform3f(glGetUniformLocation(m_program, "uCamPos"), m_camera.position.x, m_camera.position.y, m_camera.position.z);

        //glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.6f));
        //glm::vec3 lightDir = glm::normalize(glm::vec3(0.0f, -1.0f, -0.3f));
        // Light from above (positive Y = from above in shader)
        glm::vec3 lightDir = glm::normalize(glm::vec3(-0.3f, 0.8f, -0.5f));
        glUniform3f(glGetUniformLocation(m_program, "uLightDir"), lightDir.x, lightDir.y, lightDir.z);

        // Base color: slightly warm grey (works with your chocolate UI)
        //glUniform3f(glGetUniformLocation(m_program, "uBaseColor"), 0.78f, 0.75f, 0.70f);
        glUniform3f(glGetUniformLocation(m_program, "uBaseColor"), 0.65f, 0.35f, 0.85f);

        // Material knobs (procedural bump + reflection)
        glUniform1f(glGetUniformLocation(m_program, "uBumpStrength"), 0.55f);
        //glUniform1f(glGetUniformLocation(m_program, "uMetalness"), 0.15f);
        //glUniform1f(glGetUniformLocation(m_program, "uRoughness"), 0.25f);
        glUniform1f(glGetUniformLocation(m_program, "uMetalness"), 0.65f);  // More metallic
        glUniform1f(glGetUniformLocation(m_program, "uRoughness"), 0.15f);  // More shiny

        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, (void*)0);
        glBindVertexArray(0);

        glUseProgram(0);

        // Ensure we don't leak depth state into ImGui.
        glDisable(GL_DEPTH_TEST);

        // Important: do NOT swap buffers here. ImGuiHost will swap at endFrame().
    }

    void GlCubeViewRenderer::setModel(std::shared_ptr<domain::Model> /*model*/) {}
    void GlCubeViewRenderer::fitAll() {
        // Reset camera
        m_yaw = 0.0f;
        m_pitch = 0.0f;
        m_distance = 3.0f;
        m_camera.target = glm::vec3(0.0f);
        updateDerivedCamera();
    }

    void GlCubeViewRenderer::ensureFramebuffer(uint32_t width, uint32_t height) {
        width = std::max(1u, width);
        height = std::max(1u, height);

        if (!m_fbInitialized) {
            m_framebuffer.initialize((int)width, (int)height);
            m_fbInitialized = true;
            m_fbWidth = width;
            m_fbHeight = height;
            return;
        }

        if (m_fbWidth != width || m_fbHeight != height) {
            m_framebuffer.resize((int)width, (int)height);
            m_fbWidth = width;
            m_fbHeight = height;
        }
    }

    void GlCubeViewRenderer::renderToFramebuffer(void* window, uint32_t width, uint32_t height) {
        if (glfwGetCurrentContext() == nullptr) return;

        // Lazy init: if initialize() ran before a context existed, build GL resources now.
        if (!m_program || !m_vao) {
            if (!EnsureGlContextAndGlad())
                return;
            createResources();
            updateDerivedCamera();
        }

        ensureFramebuffer(width, height);

        m_framebuffer.bind();
        m_viewWidth = std::max(1u, width);
        m_viewHeight = std::max(1u, height);
        updateDerivedCamera();

        // Re-use the normal render path, but targeting the bound FBO.
        render(static_cast<GLFWwindow*>(window));
        m_framebuffer.unbind();
    }

    void* GlCubeViewRenderer::getFramebufferTexture() {
        if (!m_fbInitialized) return nullptr;
        // ImGui expects an ImTextureID which, for the OpenGL backend, is a GLuint texture id.
        return (void*)(intptr_t)m_framebuffer.colorTexture();
    }
    
    void GlCubeViewRenderer::resize(int width, int height) {
        m_viewWidth = std::max(1, width);
        m_viewHeight = std::max(1, height);
        // Keep the FBO in sync if the UI is driving viewport resizes via resize().
        if (glfwGetCurrentContext() != nullptr) {
            ensureFramebuffer((uint32_t)m_viewWidth, (uint32_t)m_viewHeight);
        }
        updateDerivedCamera();
    }

    void GlCubeViewRenderer::setScene(const ports::RenderScene& scene) { m_scene = scene; }
    void GlCubeViewRenderer::setCameraState(const ports::CameraState& camera) { m_camera = camera; updateDerivedCamera(); }
    ports::CameraState GlCubeViewRenderer::getCameraState() const { return m_camera; }

    ports::RendererCapabilities GlCubeViewRenderer::getCapabilities() const {
        ports::RendererCapabilities caps{};
        caps.supportsFramebufferTexture = true;
        caps.supportsNativeBRep = false;
        return caps;
    }

    ports::RendererBackend GlCubeViewRenderer::getBackend() const { return ports::RendererBackend::OpenGL; }

    void GlCubeViewRenderer::rotate(float dx, float dy) {
        // dx/dy expected in pixels; scale to radians
        m_yaw   += dx * 0.005f;
        m_pitch += dy * 0.005f;
        updateDerivedCamera();
    }

    void GlCubeViewRenderer::pan(float /*dx*/, float /*dy*/) {
        // Not implemented yet (will be added when you add a world-space workplane + picking)
    }

    void GlCubeViewRenderer::zoom(float delta) {
        // delta typically mouse wheel; positive = zoom in.
        // Use exponential zoom for consistent feel.
        const float factor = std::pow(0.85f, delta);
        if (m_camera.orthographic) {
            m_camera.orthoScale = std::max(0.01f, m_camera.orthoScale * factor);
        } else {
            m_distance *= factor;
        }
        updateDerivedCamera();
    }

    void GlCubeViewRenderer::setViewDirection(int direction) {
        // 0..5 like +/-X +/-Y +/-Z
        switch (direction) {
            case 0: m_yaw = 0.0f;          m_pitch = 0.0f; break;                 // +X
            case 1: m_yaw = 3.14159f;      m_pitch = 0.0f; break;                 // -X
            case 2: m_yaw = 1.57079f;      m_pitch = 0.0f; break;                 // +Z
            case 3: m_yaw = -1.57079f;     m_pitch = 0.0f; break;                 // -Z
            case 4: m_yaw = 0.0f;          m_pitch = 1.57079f; break;             // +Y
            case 5: m_yaw = 0.0f;          m_pitch = -1.57079f; break;            // -Y
            default: break;
        }
        updateDerivedCamera();
    }

} // namespace adapters

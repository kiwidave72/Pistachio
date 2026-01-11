#include "adapters/rendering/OpenGlRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>

namespace adapters {

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

    OpenGlRenderer::OpenGlRenderer() = default;

    OpenGlRenderer::~OpenGlRenderer() {
        shutdown();
    }

    bool OpenGlRenderer::initialize() {
        // Assumes an OpenGL context is current (created by the UI adapter).
        try {
            ensureProgram();

            glGenVertexArrays(1, &m_vao);
            glGenBuffers(1, &m_vbo);

            recreateFramebuffer();

            m_initialized = true;
            return true;
        } catch (...) {
            shutdown();
            return false;
        }
    }

    void OpenGlRenderer::shutdown() {
        destroyFramebuffer();
        destroyDrawResources();
        m_initialized = false;
    }

    void OpenGlRenderer::setModel(std::shared_ptr<domain::Model> model) {
        // This renderer currently ignores OCCT shapes. We'll add meshing later.
        m_model = std::move(model);
    }

    void OpenGlRenderer::fitAll() {
        // Minimal: reset camera to a reasonable default.
        m_camera = ports::CameraState{};
    }

    void* OpenGlRenderer::getFramebufferTexture() {
        // ImGui expects a texture ID cast to void*.
        return (void*)(intptr_t)m_colorTex;
    }

    void OpenGlRenderer::resize(int width, int height) {
        m_width = (width > 1) ? width : 1;
        m_height = (height > 1) ? height : 1;
        if (m_initialized) recreateFramebuffer();
    }

    void OpenGlRenderer::setScene(const ports::RenderScene& scene) {
        m_scene = scene;
    }

    void OpenGlRenderer::setCameraState(const ports::CameraState& camera) {
        m_camera = camera;
    }

    ports::CameraState OpenGlRenderer::getCameraState() const {
        return m_camera;
    }

    ports::RendererCapabilities OpenGlRenderer::getCapabilities() const {
        ports::RendererCapabilities caps;
        caps.supportsFramebufferTexture = true;
        caps.supportsNativeBRep = false;
        caps.supportsSectionPlane = false;
        return caps;
    }

    ports::RendererBackend OpenGlRenderer::getBackend() const {
        return ports::RendererBackend::OpenGL;
    }

    void OpenGlRenderer::render() {
        if (!m_initialized || m_fbo == 0) return;

        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glViewport(0, 0, m_width, m_height);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Basic grid + scene
        drawWorkplaneGrid();
        drawScenePrimitives();

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGlRenderer::rotate(float dx, float dy) {
        // Orbit around target.
        glm::vec3 dir = m_camera.position - m_camera.target;
        float r = glm::length(dir);
        if (r < 1e-3f) return;

        // Convert to spherical-ish, apply deltas
        glm::vec3 forward = glm::normalize(m_camera.target - m_camera.position);
        glm::vec3 right = glm::normalize(glm::cross(forward, m_camera.up));

        glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), -dx, m_camera.up);
        glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), -dy, right);

        glm::vec4 newDir4 = pitch * yaw * glm::vec4(dir, 0.0f);
        glm::vec3 newDir = glm::vec3(newDir4);
        m_camera.position = m_camera.target + newDir;
    }

    void OpenGlRenderer::pan(float dx, float dy) {
        // Pan in view plane (scale based on distance)
        glm::vec3 forward = glm::normalize(m_camera.target - m_camera.position);
        glm::vec3 right = glm::normalize(glm::cross(forward, m_camera.up));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));

        float dist = glm::length(m_camera.target - m_camera.position);
        float scale = dist * 0.002f;

        glm::vec3 delta = (-right * dx + up * dy) * scale;
        m_camera.position += delta;
        m_camera.target += delta;
    }

    void OpenGlRenderer::zoom(float delta) {
        glm::vec3 dir = glm::normalize(m_camera.target - m_camera.position);
        float dist = glm::length(m_camera.target - m_camera.position);
        float step = dist * delta;
        m_camera.position += dir * step;
    }

    void OpenGlRenderer::setViewDirection(int direction) {
        // Simple axis views: 0=iso, 1=top,2=front,3=right (matches existing UI assumptions)
        glm::vec3 t = m_camera.target;
        float dist = glm::length(m_camera.position - m_camera.target);
        if (dist < 1.0f) dist = 500.0f;
        switch (direction) {
        case 1: // top
            m_camera.position = t + glm::vec3(0, dist, 0);
            m_camera.up = glm::vec3(0, 0, -1);
            break;
        case 2: // front
            m_camera.position = t + glm::vec3(0, 0, dist);
            m_camera.up = glm::vec3(0, 1, 0);
            break;
        case 3: // right
            m_camera.position = t + glm::vec3(dist, 0, 0);
            m_camera.up = glm::vec3(0, 1, 0);
            break;
        default: // iso
            m_camera.position = t + glm::vec3(dist, dist, dist);
            m_camera.up = glm::vec3(0, 1, 0);
            break;
        }
    }

    void OpenGlRenderer::recreateFramebuffer() {
        destroyFramebuffer();

        glGenTextures(1, &m_colorTex);
        glBindTexture(GL_TEXTURE_2D, m_colorTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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
            destroyFramebuffer();
            throw std::runtime_error("OpenGL framebuffer incomplete");
        }
    }

    void OpenGlRenderer::destroyFramebuffer() {
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

    void OpenGlRenderer::destroyDrawResources() {
        if (m_program) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
        if (m_vbo) {
            glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        if (m_vao) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
    }

    void OpenGlRenderer::ensureProgram() {
        if (m_program) return;

        const char* vsSrc = R"GLSL(
            #version 330 core
            layout(location=0) in vec3 aPos;
            uniform mat4 uMVP;
            void main() {
                gl_Position = uMVP * vec4(aPos, 1.0);
            }
        )GLSL";

        const char* fsSrc = R"GLSL(
            #version 330 core
            out vec4 FragColor;
            uniform vec4 uColor;
            void main() {
                FragColor = uColor;
            }
        )GLSL";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
        m_program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
    }

    glm::mat4 OpenGlRenderer::makeView(const ports::CameraState& c) {
        return glm::lookAt(c.position, c.target, c.up);
    }

    glm::mat4 OpenGlRenderer::makeProj(const ports::CameraState& c, float aspect) {
        if (c.orthographic) {
            float s = c.orthoScale;
            return glm::ortho(-s * aspect, s * aspect, -s, s, c.nearPlane, c.farPlane);
        }
        return glm::perspective(c.fovYRadians, aspect, c.nearPlane, c.farPlane);
    }

    void OpenGlRenderer::drawWorkplaneGrid() {
        if (!m_scene.hasWorkplane) return;

        // Build a simple grid in plane UV, transform to world.
        const int lines = std::max(1, m_scene.workplaneGridLines);
        const float spacing = m_scene.workplaneGridSpacing;
        const float half = lines * spacing;

        std::vector<glm::vec3> verts;
        verts.reserve((size_t)(lines * 4 + 4));

        auto xform = m_scene.workplaneToWorld;
        // Offset slightly along plane normal to reduce z-fighting.
        glm::vec3 n = glm::normalize(glm::vec3(xform[2]));
        glm::vec3 bias = n * m_scene.workplaneDepthBias;

        auto toWorld = [&](float u, float v) {
            glm::vec4 p = xform * glm::vec4(u, v, 0.0f, 1.0f);
            return glm::vec3(p) + bias;
        };

        for (int i = -lines; i <= lines; ++i) {
            float t = i * spacing;
            // vertical
            verts.push_back(toWorld(t, -half));
            verts.push_back(toWorld(t, +half));
            // horizontal
            verts.push_back(toWorld(-half, t));
            verts.push_back(toWorld(+half, t));
        }

        glUseProgram(m_program);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(glm::vec3)), verts.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

        float aspect = (m_height > 0) ? (float)m_width / (float)m_height : 1.0f;
        glm::mat4 mvp = makeProj(m_camera, aspect) * makeView(m_camera);
        GLint locMVP = glGetUniformLocation(m_program, "uMVP");
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, &mvp[0][0]);

        // Grey grid
        GLint locColor = glGetUniformLocation(m_program, "uColor");
        glUniform4f(locColor, 0.3f, 0.3f, 0.3f, 1.0f);

        glLineWidth(1.0f);
        glDrawArrays(GL_LINES, 0, (GLsizei)verts.size());
    }

    void OpenGlRenderer::drawScenePrimitives() {
        if (m_scene.primitives.empty()) return;

        float aspect = (m_height > 0) ? (float)m_width / (float)m_height : 1.0f;
        glm::mat4 mvp = makeProj(m_camera, aspect) * makeView(m_camera);

        glUseProgram(m_program);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        GLint locMVP = glGetUniformLocation(m_program, "uMVP");
        glUniformMatrix4fv(locMVP, 1, GL_FALSE, &mvp[0][0]);

        GLint locColor = glGetUniformLocation(m_program, "uColor");

        for (const auto& prim : m_scene.primitives) {
            if (prim.positions.empty()) continue;
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(prim.positions.size() * sizeof(glm::vec3)), prim.positions.data(), GL_DYNAMIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

            float r = ((prim.rgba >> 24) & 0xFF) / 255.0f;
            float g = ((prim.rgba >> 16) & 0xFF) / 255.0f;
            float b = ((prim.rgba >> 8) & 0xFF) / 255.0f;
            float a = ((prim.rgba >> 0) & 0xFF) / 255.0f;
            glUniform4f(locColor, r, g, b, a);

            if (!prim.depthTest) glDisable(GL_DEPTH_TEST);
            else glEnable(GL_DEPTH_TEST);

            switch (prim.kind) {
            case ports::RenderPrimitiveKind::PointList:
                glPointSize(std::max(1.0f, prim.size));
                glDrawArrays(GL_POINTS, 0, (GLsizei)prim.positions.size());
                break;
            case ports::RenderPrimitiveKind::LineStrip:
                glLineWidth(std::max(1.0f, prim.size));
                glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)prim.positions.size());
                break;
            default:
                glLineWidth(std::max(1.0f, prim.size));
                glDrawArrays(GL_LINES, 0, (GLsizei)prim.positions.size());
                break;
            }
        }
    }

} // namespace adapters

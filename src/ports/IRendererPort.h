#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "domain/Model.h"

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace ports {

    enum class RendererBackend : uint8_t {
        Occt = 0,
        OpenGL = 1,
    };

    struct RendererCapabilities {
        bool supportsFramebufferTexture = false; // Can embed into ImGui viewport via texture
        bool supportsNativeBRep = false;          // Can display TopoDS_Shape natively
        bool supportsSectionPlane = false;
    };

    struct CameraState {
        // World-space camera
        glm::vec3 position{ 0.0f, 0.0f, 500.0f };
        glm::vec3 target{ 0.0f, 0.0f, 0.0f };
        glm::vec3 up{ 0.0f, 1.0f, 0.0f };

        bool orthographic = false;
        float fovYRadians = 0.78539816339f; // 45 deg
        float orthoScale = 200.0f;
        float nearPlane = 0.1f;
        float farPlane = 100000.0f;
    };

    enum class RenderPrimitiveKind : uint8_t {
        PointList = 0,
        LineList = 1,
        LineStrip = 2,
    };

    struct RenderPrimitive {
        RenderPrimitiveKind kind = RenderPrimitiveKind::LineList;
        std::vector<glm::vec3> positions; // World-space positions
        uint32_t rgba = 0xFFFFFFFFu;      // Packed RGBA8
        float size = 1.0f;               // Point size or line width hint
        uint64_t selectionId = 0;        // For picking later
        bool depthTest = true;
    };

    struct RenderScene {
        // A renderer-agnostic scene: mostly for sketch overlays, gizmos, and debug drawing.
        // OCCT renderer may ignore these for now.
        std::vector<RenderPrimitive> primitives;

        // Active sketch plane (optional). If enabled, renderers can draw a grid and use it
        // as a reference for sketch overlays.
        bool hasWorkplane = false;
        glm::mat4 workplaneToWorld{ 1.0f }; // Plane UV -> World transform
        float workplaneGridSpacing = 10.0f;
        int workplaneGridLines = 20;       // lines per side
        float workplaneDepthBias = 0.25f;  // world units offset along plane normal
    };

    class IRendererPort {
    public:
        virtual ~IRendererPort() = default;

        virtual bool initialize() = 0;
        virtual void shutdown() = 0;
        virtual void render(GLFWwindow* m_window) = 0;

        // Optional: BRep / model rendering (OCCT-backed)
        virtual void setModel(std::shared_ptr<domain::Model> model) = 0;
        virtual void fitAll() = 0;

        // Optional: embed viewport via texture (OpenGL-backed)
        virtual void* getFramebufferTexture() = 0;
        virtual void resize(int width, int height) = 0;

        // Unified scene + camera (renderer-agnostic). Safe to call every frame.
        virtual void setScene(const RenderScene& scene) = 0;
        virtual void setCameraState(const CameraState& camera) = 0;
        virtual CameraState getCameraState() const = 0;
        virtual RendererCapabilities getCapabilities() const = 0;
        virtual RendererBackend getBackend() const = 0;

        // Camera controls (legacy UI hooks)
        virtual void rotate(float dx, float dy) = 0;
        virtual void pan(float dx, float dy) = 0;
        virtual void zoom(float delta) = 0;
        virtual void setViewDirection(int direction) = 0;
    };

} // namespace ports

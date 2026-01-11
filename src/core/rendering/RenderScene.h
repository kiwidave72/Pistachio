#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>

namespace core::rendering
{
    using RenderId = std::uint64_t;

    struct Color
    {
        float r{ 1 }, g{ 1 }, b{ 1 }, a{ 1 };
    };

    struct Line3D
    {
        RenderId id{};
        glm::vec3 a;
        glm::vec3 b;
        Color color;
        float thickness{ 1.0f };
        bool selectable{ true };
    };

    struct Point3D
    {
        RenderId id{};
        glm::vec3 p;
        Color color;
        float size{ 4.0f };
        bool selectable{ true };
    };

    struct Polyline3D
    {
        RenderId id{};
        std::vector<glm::vec3> points;
        Color color;
        float thickness{ 1.0f };
        bool selectable{ true };
    };

    // --- NEW: true curve primitives (renderer-agnostic) ---

    struct Circle3D
    {
        RenderId id{};
        glm::vec3 center{ 0,0,0 };
        glm::vec3 normal{ 0,0,1 };   // plane normal
        float radius{ 1.0f };
        Color color;
        float thickness{ 1.0f };
        bool selectable{ true };
        bool construction{ false };
    };

    struct Arc3D
    {
        RenderId id{};
        glm::vec3 center{ 0,0,0 };
        glm::vec3 normal{ 0,0,1 };
        float radius{ 1.0f };

        // Start/end points in the arc plane (world coords)
        glm::vec3 start{ 1,0,0 };
        glm::vec3 end{ 0,1,0 };

        bool ccw{ true };

        Color color;
        float thickness{ 1.0f };
        bool selectable{ true };
        bool construction{ false };
    };

    struct Ellipse3D
    {
        RenderId id{};
        glm::vec3 center{ 0,0,0 };
        glm::vec3 normal{ 0,0,1 };

        float rx{ 2.0f };
        float ry{ 1.0f };

        // Rotation about normal (radians). 0 means major axis along +X in plane.
        float rotationRad{ 0.0f };

        Color color;
        float thickness{ 1.0f };
        bool selectable{ true };
        bool construction{ false };
    };

    struct GridPlane
    {
        glm::vec3 origin;
        glm::vec3 uAxis;
        glm::vec3 vAxis;
        float spacing{ 10.0f };
        int lineCount{ 20 };
        Color majorColor{ 0.4f, 0.4f, 0.4f, 1.0f };
        Color minorColor{ 0.2f, 0.2f, 0.2f, 1.0f };
        bool visible{ true };
    };

    struct RenderScene
    {
        std::vector<Point3D>    points;
        std::vector<Line3D>     lines;
        std::vector<Polyline3D> polylines;

        // NEW
        std::vector<Circle3D>   circles;
        std::vector<Arc3D>      arcs;
        std::vector<Ellipse3D>  ellipses;

        GridPlane grid;

        bool showGrid{ true };
        bool ghostSolids{ false };
        bool sectionActive{ false };

        void Clear()
        {
            points.clear();
            lines.clear();
            polylines.clear();
            circles.clear();
            arcs.clear();
            ellipses.clear();
        }
    };
}

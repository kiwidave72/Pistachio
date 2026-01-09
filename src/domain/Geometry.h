#pragma once
#include <vector>
#include <array>

namespace domain {

    using Point3D = std::array<float, 3>;
    using Triangle = std::array<size_t, 3>;

    class Geometry {
    public:
        Geometry() = default;

        void addVertex(const Point3D& vertex);
        void addTriangle(const Triangle& triangle);

        const std::vector<Point3D>& getVertices() const;
        const std::vector<Triangle>& getTriangles() const;

        void clear();
        bool isEmpty() const;

    private:
        std::vector<Point3D> m_vertices;
        std::vector<Triangle> m_triangles;
    };

} // namespace domain
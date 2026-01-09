#include "domain/Geometry.h"

namespace domain {

    void Geometry::addVertex(const Point3D& vertex) {
        m_vertices.push_back(vertex);
    }

    void Geometry::addTriangle(const Triangle& triangle) {
        m_triangles.push_back(triangle);
    }

    const std::vector<Point3D>& Geometry::getVertices() const {
        return m_vertices;
    }

    const std::vector<Triangle>& Geometry::getTriangles() const {
        return m_triangles;
    }

    void Geometry::clear() {
        m_vertices.clear();
        m_triangles.clear();
    }

    bool Geometry::isEmpty() const {
        return m_vertices.empty();
    }

} // namespace domain
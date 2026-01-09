#include "domain/Model.h"

namespace domain {

    Model::Model(const std::string& name) : m_name(name) {}

    void Model::setName(const std::string& name) {
        m_name = name;
    }

    std::string Model::getName() const {
        return m_name;
    }

    void Model::addGeometry(std::shared_ptr<Geometry> geometry) {
        if (geometry) {
            m_geometries.push_back(geometry);
        }
    }

    const std::vector<std::shared_ptr<Geometry>>& Model::getGeometries() const {
        return m_geometries;
    }

    void Model::clearGeometries() {
        m_geometries.clear();
    }

    bool Model::isEmpty() const {
        return m_geometries.empty() && !m_occtShape;
    }

    size_t Model::getGeometryCount() const {
        return m_geometries.size();
    }

    void Model::setOcctShape(const std::shared_ptr<TopoDS_Shape>& shape) {
        m_occtShape = shape;
    }

    std::shared_ptr<TopoDS_Shape> Model::getOcctShape() const {
        return m_occtShape;
    }

    bool Model::hasOcctShape() const {
        return m_occtShape != nullptr;
    }

} // namespace domain
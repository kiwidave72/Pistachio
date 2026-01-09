#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Geometry.h"

// Forward declarations for OpenCASCADE types
class TopoDS_Shape;

namespace domain {

    class Model {
    public:
        Model() = default;
        explicit Model(const std::string& name);

        void setName(const std::string& name);
        std::string getName() const;

        void addGeometry(std::shared_ptr<Geometry> geometry);
        const std::vector<std::shared_ptr<Geometry>>& getGeometries() const;
        void clearGeometries();

        bool isEmpty() const;
        size_t getGeometryCount() const;

        // OpenCASCADE integration
        void setOcctShape(const std::shared_ptr<TopoDS_Shape>& shape);
        std::shared_ptr<TopoDS_Shape> getOcctShape() const;
        bool hasOcctShape() const;

    private:
        std::string m_name;
        std::vector<std::shared_ptr<Geometry>> m_geometries;
        std::shared_ptr<TopoDS_Shape> m_occtShape;
    };

} // namespace domain
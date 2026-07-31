#include "domain/Mesh.hpp"
#include <limits>

namespace slicer {

Vec3 Mesh::bboxMin() const noexcept {
    Vec3 mn{ std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max() };
    for (const auto& v : vertices) {
        if (v.x < mn.x) mn.x = v.x;
        if (v.y < mn.y) mn.y = v.y;
        if (v.z < mn.z) mn.z = v.z;
    }
    return mn;
}

Vec3 Mesh::bboxMax() const noexcept {
    Vec3 mx{ std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest() };
    for (const auto& v : vertices) {
        if (v.x > mx.x) mx.x = v.x;
        if (v.y > mx.y) mx.y = v.y;
        if (v.z > mx.z) mx.z = v.z;
    }
    return mx;
}

} // namespace slicer

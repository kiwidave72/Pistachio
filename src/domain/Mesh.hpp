#ifndef MESH_HPP
#define MESH_HPP

#include <cmath>
#include <stdexcept>
#include <iostream>

class Vec3 {
public:
    double x, y, z;

    // Constructors
    constexpr Vec3() : x(0), y(0), z(0) {}
    constexpr Vec3(double x, double y, double z) : x(x), y(y), z(z) {}

    // Addition
    constexpr Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    // Subtraction
    constexpr Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    // Scalar multiplication
    constexpr Vec3 operator*(double scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }

    // Scalar division with validation
    Vec3 operator/(double scalar) const {
        if (scalar == 0.0) throw std::runtime_error("Division by zero in Vec3");
        return Vec3(x / scalar, y / scalar, z / scalar);
    }

    // Dot product
    constexpr double dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    // Cross product
    constexpr Vec3 cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    // Magnitude (length)
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    // Normalization
    Vec3 normalized() const {
        double len = length();
        if (len == 0.0) throw std::runtime_error("Cannot normalize zero-length vector");
        return *this / len;
    }

    // Equality check
    constexpr bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Vec3& v) {
        return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
    }
};

 
namespace slicer {

    class Mesh {
    public:
        Vec3 bboxMin() const noexcept;
        Vec3 bboxMax() const noexcept;

    private:
        std::vector<Vec3> vertices;
    };
}
#endif 
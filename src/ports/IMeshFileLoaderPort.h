#pragma once
#include <memory>
#include <string>
#include <functional>
#include "domain/Mesh.hpp"

namespace ports {

    // Progress callback: (message, percentage 0-100)
    using ProgressCallback = std::function<void(const std::string&, float)>;

    class IMeshFileLoaderPort {
    public:
        virtual ~IMeshFileLoaderPort() = default;

       virtual std::shared_ptr<slicer::Mesh> load(
            const std::string& filepath,
            ProgressCallback progressCallback = nullptr
        ) = 0;

        virtual bool canLoad(const std::string& filepath) const = 0;
        virtual std::string getSupportedExtensions() const = 0;
    };

} // namespace ports
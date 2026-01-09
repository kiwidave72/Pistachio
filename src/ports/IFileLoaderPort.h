#pragma once
#include <memory>
#include <string>
#include <functional>
#include "domain/Model.h"

namespace ports {

    // Progress callback: (message, percentage 0-100)
    using ProgressCallback = std::function<void(const std::string&, float)>;

    class IFileLoaderPort {
    public:
        virtual ~IFileLoaderPort() = default;

        virtual std::shared_ptr<domain::Model> load(
            const std::string& filepath,
            ProgressCallback progressCallback = nullptr
        ) = 0;

        virtual bool canLoad(const std::string& filepath) const = 0;
        virtual std::string getSupportedExtensions() const = 0;
    };

} // namespace ports
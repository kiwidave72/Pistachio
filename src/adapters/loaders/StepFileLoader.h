#pragma once
#include "ports/IFileLoaderPort.h"

namespace adapters {

    class StepFileLoader : public ports::IFileLoaderPort {
    public:
        std::shared_ptr<domain::Model> load(
            const std::string& filepath,
            ports::ProgressCallback progressCallback = nullptr
        ) override;

        bool canLoad(const std::string& filepath) const override;
        std::string getSupportedExtensions() const override;

    private:
        bool hasStepExtension(const std::string& filepath) const;
    };

} // namespace adapters
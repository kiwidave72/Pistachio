#pragma once
#include "ports/IExporterPort.h"

namespace adapters {

class StlExporter : public ports::IExporterPort {
public:
    bool exportModel(const domain::Model& model, const std::string& filepath) override;
    std::string getSupportedExtension() const override;
};

} // namespace adapters
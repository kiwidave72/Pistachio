#pragma once
#include <memory>
#include <string>
#include "domain/Model.h"

namespace ports {

class IExporterPort {
public:
    virtual ~IExporterPort() = default;
    
    virtual bool exportModel(const domain::Model& model, const std::string& filepath) = 0;
    virtual std::string getSupportedExtension() const = 0;
};

} // namespace ports
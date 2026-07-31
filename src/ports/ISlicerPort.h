#pragma once
#include <memory>
#include <string>
#include "domain/Model.h"
#include "ports/IConfigPort.h"

namespace ports {

   using SlicerProgressCallback = std::function<void(const std::string&, float)>;

    class ISlicerPort {
    public:
         ~ISlicerPort() = default;

        virtual bool initialize(ports::IConfigPort& config) =0;

        virtual bool sliceModel(const std::shared_ptr<domain::Model>  model, const std::string& filepath,  SlicerProgressCallback progressCallback = nullptr) =0 ;
    };

} // namespace ports
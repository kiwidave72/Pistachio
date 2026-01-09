#include "adapters/loaders/StepFileLoader.h"
#include <algorithm>
#include <stdexcept>
#include <cctype>

#include <STEPControl_Reader.hxx>
#include <TopoDS_Shape.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <BRepMesh_IncrementalMesh.hxx>

namespace adapters {

    std::shared_ptr<domain::Model> StepFileLoader::load(
        const std::string& filepath,
        ports::ProgressCallback progressCallback)
    {
        if (progressCallback) {
            progressCallback("Reading STEP file...", 10.0f);
        }

        STEPControl_Reader reader;

        IFSelect_ReturnStatus status = reader.ReadFile(filepath.c_str());

        if (status != IFSelect_RetDone) {
            throw std::runtime_error("Failed to read STEP file: " + filepath);
        }

        if (progressCallback) {
            progressCallback("Transferring shapes...", 40.0f);
        }

        Standard_Integer nbRoots = reader.TransferRoots();
        if (nbRoots == 0) {
            throw std::runtime_error("No shapes found in STEP file");
        }

        if (progressCallback) {
            progressCallback("Building geometry...", 60.0f);
        }

        TopoDS_Shape shape = reader.OneShape();

        if (shape.IsNull()) {
            throw std::runtime_error("Loaded shape is null");
        }

        auto model = std::make_shared<domain::Model>(filepath);
        model->setOcctShape(std::make_shared<TopoDS_Shape>(shape));

        if (progressCallback) {
            progressCallback("Generating mesh...", 80.0f);
        }

        try {
            BRepMesh_IncrementalMesh mesh(shape, 0.1);
            if (mesh.IsDone()) {
                // Mesh generated successfully
            }
        }
        catch (...) {
            // Meshing failed, but we still have the shape
        }

        if (progressCallback) {
            progressCallback("Complete!", 100.0f);
        }

        return model;
    }

    bool StepFileLoader::canLoad(const std::string& filepath) const {
        return hasStepExtension(filepath);
    }

    std::string StepFileLoader::getSupportedExtensions() const {
        return ".step, .stp";
    }

    bool StepFileLoader::hasStepExtension(const std::string& filepath) const {
        if (filepath.length() < 4) return false;

        std::string lower = filepath;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });

        return lower.substr(lower.length() - 5) == ".step" ||
            lower.substr(lower.length() - 4) == ".stp";
    }

} // namespace adapters
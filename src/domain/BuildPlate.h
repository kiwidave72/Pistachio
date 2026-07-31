#pragma once

#include <string>
#include <vector>

#include "ModelCache.h"
#include "ModelInstance.h"

#include <nlohmann/json.hpp> // Include the nlohmann/json library

namespace domain::v1 {

   

    class BuildPlate
    {
    public:
		BuildPlate();
		~BuildPlate();

        std::string Id;
        std::string name;
        std::string label;
       
        std::vector<std::unique_ptr<ModelInstance>> modelInstances;
        std::shared_ptr<domain::v1::Model> buildPlateModel;

    };

    // 1. Convert BuildPlate instance to JSON
    inline void to_json(nlohmann::json& j, const BuildPlate& b) {
        j["Id"] = b.Id;
        j["name"] = b.name;
        j["label"] = b.label;


        // Serialize models pointer array safely
        

        j["instances"] = nlohmann::json::array();

        for (const auto& instance : b.modelInstances)
        {
            j["instances"].push_back(*instance);
        }
        
    }

    // 2. Convert JSON back into a BuildPlate instance
    inline void from_json(const nlohmann::json& j, BuildPlate& b) {
        // Clear out any old pointer elements to prevent memory leaks if overwriting
       
 
        // Deserialize standard fields safely
        if (j.contains("Id") && j["Id"].is_string()) {
            j["Id"].get_to(b.Id);
        }
        if (j.contains("name") && j["name"].is_string()) {
            j["name"].get_to(b.name);
        }
        if (j.contains("label") && j["label"].is_string()) {
            j["label"].get_to(b.label);
        }

        // Deserialize models pointer array safely
       

        b.modelInstances.clear();

        if (!j.contains("instances") || !j["instances"].is_array())
            return;

        for (const auto& item : j["instances"])
        {
            auto instance = std::make_unique<ModelInstance>();

            // let ModelInstance deserialize itself
            *instance = item.get<ModelInstance>();

            b.modelInstances.push_back(std::move(instance));
        }
    }
}
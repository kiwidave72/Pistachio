#pragma once

#include <string>
#include <vector>

#include "ModelCache.h"
#include <nlohmann/json.hpp>  

namespace domain::v1 {



    class ModelInstance
    {
    public:

        ModelInstance();
        explicit ModelInstance(std::string existingId);
        ~ModelInstance() = default;

        // Identity
        std::string id;
        std::string name;
        std::string modelHash;

        // Placement
        Transform transform;
     
        glm::vec3 color{ 0.65f, 0.35f, 0.85f }; // Default purple

      
    private:


    };



    inline void to_json(nlohmann::json& j, const ModelInstance& instance)
    {
        j =
        {
            { "id", instance.id },
            { "name", instance.name },
            { "modelHash", instance.modelHash },

            { "transform", instance.transform }

        };
    }

    inline void from_json(const nlohmann::json& j, ModelInstance& instance)
    {
        if (j.contains("id"))
            j.at("id").get_to(instance.id);

        if (j.contains("name"))
            j.at("name").get_to(instance.name);

        if (j.contains("modelHash"))
            j.at("modelHash").get_to(instance.modelHash);

        if (j.contains("transform"))
            j.at("transform").get_to(instance.transform);

    }
}
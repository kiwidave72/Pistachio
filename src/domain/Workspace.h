#pragma once
#include <vector>
#include <nlohmann/json.hpp>
#include "Project.h"
#include "Repository.h"
#include "ModelCache.h"

using namespace domain::v1;

namespace domain::v1 {


    class Workspace
    {
    public:
        Workspace();
        ~Workspace();

        std::vector<domain::v1::Project*> projects;
        std::vector<domain::v1::Repository*> repositories;

    };

    // Define manual serialization functions below the class
    inline void to_json(nlohmann::json& j, const Workspace& w) {
        // 1. Serialize projects array safely
        j["projects"] = nlohmann::json::array();
        for (const auto* proj : w.projects) {
            if (proj) {
                j["projects"].push_back(*proj); // Dereferences and calls Project's serializer
            }
            else {
                j["projects"].push_back(nullptr);
            }
        }


    }

    inline void from_json(const nlohmann::json& j, Workspace& w) {
        // Clear out old elements to prevent memory leaks if parsing over an existing object
        for (auto* p : w.projects) delete p;
        for (auto* r : w.repositories) delete r;
        w.projects.clear();
        w.repositories.clear();

        // 1. Deserialize projects
        if (j.contains("projects") && j["projects"].is_array()) {
            for (const auto& element : j["projects"]) {
                if (element.is_null()) {
                    w.projects.push_back(nullptr);
                }
                else {
                    domain::v1::Project* proj = new domain::v1::Project();
                    element.get_to(*proj);
                    w.projects.push_back(proj);
                }
            }
        }




    }
}

#pragma once

// -----------------------------------------------------------------------
// GlmJson.h
//
// glm::vec3 <-> nlohmann::json conversion  ONE canonical location,
// included directly by anything that needs it (Transform.h,
// ModelCache.h, etc.), rather than relying on transitive include order
// between those files. This is what kept flip-flopping between
// "duplicate definition" and "no conversion found"  the actual fix is
// making this its own explicit dependency, not bundled inside either.
// -----------------------------------------------------------------------

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace glm {
    inline void to_json(nlohmann::json& j, const glm::vec3& v)
    {
        j = { v.x, v.y, v.z };
    }

    // Defensive on purpose: every other from_json in this codebase checks
    // shape before extracting (see Model/BuildPlate/Project/ModelInstance/
    // Transform), but this one didn't — it went straight to j.at(0/1/2),
    // which throws nlohmann::json::out_of_range (or type_error, if j isn't
    // even an array) with nothing upstream catching it. Since this sits at
    // the bottom of the Workspace -> Project -> BuildPlate -> ModelInstance
    // -> Transform -> vec3 deserialization chain, any single malformed/
    // missing/wrong-shape "position"/"rotation"/"scale" field anywhere in
    // a saved workspace.json takes down the whole app via j.get_to(ws).
    //
    // Two valid shapes are accepted, since both show up in practice:
    //   - array:  [x, y, z]           <- what to_json() above emits
    //   - object: {"x":.., "y":.., "z":..}  <- hand-written/older workspace
    //             files use this; silently zeroing it out here would be
    //             quiet data loss for perfectly valid input, not safety.
    // Anything else (wrong size, wrong types, null) degrades to a zero
    // vector rather than throwing.
    inline void from_json(const nlohmann::json& j, glm::vec3& v)
    {
        auto num = [](const nlohmann::json& field) -> float
            {
                return field.is_number() ? field.get<float>() : 0.0f;
            };

        if (j.is_array() && j.size() >= 3)
        {
            v.x = num(j.at(0));
            v.y = num(j.at(1));
            v.z = num(j.at(2));
            return;
        }

        if (j.is_object())
        {
            v.x = j.contains("x") ? num(j.at("x")) : 0.0f;
            v.y = j.contains("y") ? num(j.at("y")) : 0.0f;
            v.z = j.contains("z") ? num(j.at("z")) : 0.0f;
            return;
        }

        v = glm::vec3(0.0f);
    }
}
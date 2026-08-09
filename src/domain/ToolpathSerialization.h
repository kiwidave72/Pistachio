#pragma once

// -----------------------------------------------------------------------
// ToolpathSerialization.h
//
// JSON save/load for domain::v1::Toolpath — lets ToolpathVisualizationPlugin
// (or a standalone dev tool) load a previously-generated toolpath without
// running the full P0-P6 pipeline every time, for fast ribbon-rendering
// iteration. Uses nlohmann::json, already a project-wide dependency.
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"

#include <nlohmann/json.hpp>

namespace domain::v1 {

    inline void to_json(nlohmann::json& j, const ToolpathVertex& v)
    {
        j = { {"x", v.position.x}, {"y", v.position.y}, {"z", v.position.z}, {"feedRate", v.feedRate} };
    }

    inline void from_json(const nlohmann::json& j, ToolpathVertex& v)
    {
        v.position = { j.at("x").get<float>(), j.at("y").get<float>(), j.at("z").get<float>() };
        v.feedRate = j.value("feedRate", 0.0f);
    }

    inline void to_json(nlohmann::json& j, const ToolpathSegment& s)
    {
        j = {
            {"start", s.start}, {"end", s.end},
            {"extrusionDelta", s.extrusionDelta}, {"extrusionWidth", s.extrusionWidth},
            {"moveType", (int)s.moveType}
        };
    }

    inline void from_json(const nlohmann::json& j, ToolpathSegment& s)
    {
        s.start = j.at("start").get<ToolpathVertex>();
        s.end = j.at("end").get<ToolpathVertex>();
        s.extrusionDelta = j.value("extrusionDelta", 0.0f);
        s.extrusionWidth = j.value("extrusionWidth", 0.0f);
        s.moveType = (ToolpathMoveType)j.value("moveType", 0);
    }

    inline void to_json(nlohmann::json& j, const ToolpathLayer& l)
    {
        j = { {"z", l.z}, {"segments", l.segments}, {"comments", l.comments} };
    }

    inline void from_json(const nlohmann::json& j, ToolpathLayer& l)
    {
        l.z = j.value("z", 0.0f);
        l.segments = j.value("segments", std::vector<ToolpathSegment>{});
        l.comments = j.value("comments", std::vector<std::string>{});
    }

    inline void to_json(nlohmann::json& j, const Toolpath& t)
    {
        j = { {"layers", t.layers} };
    }

    inline void from_json(const nlohmann::json& j, Toolpath& t)
    {
        t.layers = j.value("layers", std::vector<ToolpathLayer>{});
    }

    // Direct file save/load — no instance/plate concept needed here,
    // just the raw Toolpath data.
    inline bool saveToolpathToFile(const Toolpath& t, const std::string& path)
    {
        nlohmann::json j = t;
        std::ofstream f(path);
        if (!f) return false;
        f << j.dump(2);
        return true;
    }

    inline bool loadToolpathFromFile(Toolpath& t, const std::string& path)
    {
        std::ifstream f(path);
        if (!f) return false;
        nlohmann::json j;
        f >> j;
        t = j.get<Toolpath>();
        return true;
    }

} // namespace domain::v1
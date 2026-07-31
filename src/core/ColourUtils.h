#pragma once
#include <string>
#include <imgui.h>

namespace utils {

    // "#RRGGBBAA" → ImVec4 (0..1 components)
    inline ImVec4 hexToImVec4(const std::string& hex)
    {
        // Strip leading '#'
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);

        // Pad to 8 chars if alpha missing (treat as fully opaque)
        if (h.size() == 6) h += "FF";
        if (h.size() != 8) return ImVec4(1, 1, 1, 1); // fallback white

        auto toF = [&](int offset) -> float {
            unsigned int v = 0;
            sscanf(h.c_str() + offset, "%2x", &v);
            return v / 255.0f;
            };

        return ImVec4(toF(0), toF(2), toF(4), toF(6));
    }

    // ImVec4 (0..1) → "#RRGGBBAA"
    inline std::string imVec4ToHex(const ImVec4& c)
    {
        auto toU = [](float f) -> unsigned int {
            int v = (int)(f * 255.0f + 0.5f);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            return (unsigned int)v;
            };

        char buf[10];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X",
            toU(c.x), toU(c.y), toU(c.z), toU(c.w));
        return buf;
    }

} // namespace utils
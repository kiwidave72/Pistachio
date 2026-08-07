#pragma once

#include <string>
#include <glm/glm.hpp>
 

namespace domain::v1 {

    enum class DiagnosticSeverity { Info, Warning, Error };

    struct DiagnosticMessage
    {
        DiagnosticSeverity severity = DiagnosticSeverity::Info;
        std::string phase;       // "P4 ExtractionPhase"
        std::string message;     // "dead-end (forward), chain had 152 points"
        bool hasLocation = false;
        glm::vec3 location{ 0.0f };   // where on the layer this applies, if relevant
    };

} // namespace domain::v1
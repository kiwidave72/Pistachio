#ifndef DOMAIN_V1_SECONDARY_PASS_REPORT_H
#define DOMAIN_V1_SECONDARY_PASS_REPORT_H

// -----------------------------------------------------------------------
// SecondaryPassReport.h
//
// Reports computation SHAPE, not per-layer data — makes it visible in
// run.json which passes are simple per-layer work (cheap, O(layers))
// versus cross-layer passes (potentially more expensive, candidates for
// future optimization) — rather than that distinction only living in a
// code comment someone has to go find.
// -----------------------------------------------------------------------

#include <string>

namespace domain::v1 {

    struct SecondaryPassReport
    {
        std::string phase;          // "P6 SkinDetection"
        std::string description;    // what it does and why it's a secondary pass
        int layersProcessed = 0;
        std::string note;           // optimization-relevant observation, if any
    };

} // namespace domain::v1

#endif // DOMAIN_V1_SECONDARY_PASS_REPORT_H
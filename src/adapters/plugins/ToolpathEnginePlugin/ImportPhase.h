#pragma once

// -----------------------------------------------------------------------
// ImportPhase.h — P0
//
// Converts a BuildPlate's ModelInstances into UnifiedGeometry, one per
// instance. Reuses already-loaded mesh data via ModelCache (no fresh
// StlLoaderAdapter reload) — see the design doc's "P0 geometry source"
// decision.
//
// Applies ModelInstance::transform AND the bed-center-to-bed-corner
// coordinate conversion in one combined matrix per instance — see
// UnifiedGeometry.h's header comment for why bed-corner-origin is used.
//
// TEMPORARY SCAFFOLDING: the corner-offset step here is expected to be
// deleted entirely once ModelInstance/the app's scene convention is
// migrated to bed-corner-origin natively. Not a permanent design
// element — don't build further logic that depends on this conversion
// happening here specifically.
// -----------------------------------------------------------------------

#include "domain/UnifiedGeometry.h"
#include "domain/ModelCache.h"
#include "domain/BuildPlate.h"
#include "ports/IConfigPort.h"

#include <vector>

namespace kinetica {

    class ImportPhase
    {
    public:
        ImportPhase(domain::v1::ModelCache& modelCache, ports::IConfigPort& config)
            : m_modelCache(modelCache), m_config(config) {
        }

        std::vector<domain::v1::UnifiedGeometry> run(domain::v1::BuildPlate* buildPlate);

    private:
        domain::v1::ModelCache& m_modelCache;
        ports::IConfigPort& m_config;
    };

} // namespace kinetica
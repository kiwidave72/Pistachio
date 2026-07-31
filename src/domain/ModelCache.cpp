#include "ModelCache.h"

namespace domain::v1 {

    // ---- Models ----
    std::shared_ptr<Model> ModelCache::getModel(const std::string& hash) const
    {
        auto it = models.find(hash);
        return (it != models.end()) ? it->second : nullptr;
    }
}
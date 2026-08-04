#include "ModelCache.h"
#include <mutex>
namespace domain::v1 {

    // ---- Models ----
    std::shared_ptr<Model> ModelCache::getModel(const std::string& hash) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = models.find(hash);
        return (it != models.end()) ? it->second : nullptr;
    }
}
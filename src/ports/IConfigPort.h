#pragma once

#include <string>
#include <vector>

// We already depend on nlohmann::json across the codebase (SketchDocumentJson, etc.)
#include <nlohmann/json.hpp>

namespace ports {

// A small, plugin-friendly settings registry.
//
// Goals:
// - Host owns the store (persists across UI hot-reloads)
// - Plugins can register typed settings (metadata + defaults)
// - Values are stored as JSON so we can extend types without rewriting persistence
// - UI can list/edit settings without knowing about each plugin

enum class SettingType {
    Bool,
    Int,
    Float,
    String,
    Json
};

struct SettingInfo {
    std::string ns;           // e.g. "pistachio.sketch"
    std::string key;          // e.g. "grid.spacing"
    std::string displayName;  // e.g. "Grid Spacing"
    std::string description;  // user-facing help
    std::string group;        // grouping in UI (optional)
    SettingType type = SettingType::Json;
    nlohmann::json defaultValue;
    bool advanced = false;

    SettingInfo() = default;

    // Preferred ctor (matches existing call-sites in Application.cpp / UI plugin)
    SettingInfo(std::string ns_,
                std::string key_,
                std::string displayName_,
                std::string description_,
                std::string group_,
                SettingType type_,
                nlohmann::json defaultValue_,
                bool advanced_ = false)
        : ns(std::move(ns_))
        , key(std::move(key_))
        , displayName(std::move(displayName_))
        , description(std::move(description_))
        , group(std::move(group_))
        , type(type_)
        , defaultValue(std::move(defaultValue_))
        , advanced(advanced_) {}

    // Convenience ctor (no group supplied)
    SettingInfo(std::string ns_,
                std::string key_,
                std::string displayName_,
                std::string description_,
                SettingType type_,
                nlohmann::json defaultValue_,
                bool advanced_ = false)
        : SettingInfo(std::move(ns_), std::move(key_), std::move(displayName_), std::move(description_),
                      std::string{}, type_, std::move(defaultValue_), advanced_) {}
};

class IConfigPort {
public:
    virtual ~IConfigPort() = default;

    // Registration is idempotent; repeated registrations overwrite metadata but preserve stored value if any.
    virtual void registerSetting(const SettingInfo& info) = 0;

    virtual bool has(const std::string& ns, const std::string& key) const = 0;

    // Returns either stored value OR default value if missing.
    virtual nlohmann::json get(const std::string& ns, const std::string& key) const = 0;

    // Sets a value (must be JSON compatible with the setting type; UI enforces type on edit).
    virtual void set(const std::string& ns, const std::string& key, const nlohmann::json& value) = 0;

    // Reset a value back to the registered default.
    virtual void resetToDefault(const std::string& ns, const std::string& key) = 0;

    // Enumerate registered settings (metadata).
    virtual std::vector<SettingInfo> listSettings() const = 0;
    virtual std::vector<SettingInfo> listSettings(const std::string& ns) const = 0;

    // Persistence for values only (metadata is re-registered on startup / plugin load).
    virtual bool loadFromFile(const std::string& path) = 0;
    virtual bool saveToFile(const std::string& path) const = 0;
};

} // namespace ports

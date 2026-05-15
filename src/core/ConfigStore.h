#pragma once
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "ports/IConfigPort.h"

namespace core {

class ConfigStore final : public ports::IConfigPort
{
public:
    ConfigStore() = default;
    ~ConfigStore() override = default;

    void registerSetting(const ports::SettingInfo& info) override;

    bool has(const std::string& ns, const std::string& key) const override;
    nlohmann::json get(const std::string& ns, const std::string& key) const override;
    void set(const std::string& ns, const std::string& key, const nlohmann::json& value) override;

    void resetToDefault(const std::string& ns, const std::string& key) override;

    std::vector<ports::SettingInfo> listSettings() const override;
    std::vector<ports::SettingInfo> listSettings(const std::string& ns) const override;

    bool loadFromFile(const std::string& path) override;
    bool saveToFile(const std::string& path) const override;

private:
    static std::string makeId(const std::string& ns, const std::string& key);

    mutable std::mutex m_mutex;

    // registry: id -> SettingInfo
    std::unordered_map<std::string, ports::SettingInfo> m_registry;

    // values: id -> json value (only for values that differ from default or were loaded)
    std::unordered_map<std::string, nlohmann::json> m_values;
};

} // namespace core

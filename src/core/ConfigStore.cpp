#include "core/ConfigStore.h"

#include <fstream>
#include <algorithm>

namespace core {

std::string ConfigStore::makeId(const std::string& ns, const std::string& key)
{
    return ns + ":" + key;
}

void ConfigStore::registerSetting(const ports::SettingInfo& info)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto id = makeId(info.ns, info.key);

    // If already registered, keep existing registered metadata but allow updating display/description/category/default.
    m_registry[id] = info;

    // Ensure a default exists
    if (m_registry[id].defaultValue.is_null())
    {
        switch (m_registry[id].type)
        {
            case ports::SettingType::Bool:   m_registry[id].defaultValue = false; break;
            case ports::SettingType::Int:    m_registry[id].defaultValue = 0; break;
            case ports::SettingType::Float:  m_registry[id].defaultValue = 0.0; break;
            case ports::SettingType::String: m_registry[id].defaultValue = ""; break;
        }
    }
}

bool ConfigStore::has(const std::string& ns, const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto id = makeId(ns, key);
    return m_registry.find(id) != m_registry.end();
}

nlohmann::json ConfigStore::get(const std::string& ns, const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto id = makeId(ns, key);

    // Prefer explicit value
    auto itVal = m_values.find(id);
    if (itVal != m_values.end())
        return itVal->second;

    // Fallback to default if registered
    auto itReg = m_registry.find(id);
    if (itReg != m_registry.end())
        return itReg->second.defaultValue;

    // Unknown setting: return null (caller can decide)
    return nullptr;
}

void ConfigStore::set(const std::string& ns, const std::string& key, const nlohmann::json& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto id = makeId(ns, key);

    // If registered and value equals default, we can drop it from overrides
    auto itReg = m_registry.find(id);
    if (itReg != m_registry.end())
    {
        if (!itReg->second.defaultValue.is_null() && value == itReg->second.defaultValue)
        {
            m_values.erase(id);
            return;
        }
    }

    m_values[id] = value;
}

void ConfigStore::resetToDefault(const std::string& ns, const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto id = makeId(ns, key);
    m_values.erase(id);
}

std::vector<ports::SettingInfo> ConfigStore::listSettings() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ports::SettingInfo> out;
    out.reserve(m_registry.size());
    for (const auto& kv : m_registry)
        out.push_back(kv.second);

    std::sort(out.begin(), out.end(), [](const ports::SettingInfo& a, const ports::SettingInfo& b) {
        if (a.ns != b.ns) return a.ns < b.ns;
        return a.key < b.key;
    });

    return out;
}

std::vector<ports::SettingInfo> ConfigStore::listSettings(const std::string& ns) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ports::SettingInfo> out;
    for (const auto& kv : m_registry)
    {
        if (kv.second.ns == ns)
            out.push_back(kv.second);
    }

    std::sort(out.begin(), out.end(), [](const ports::SettingInfo& a, const ports::SettingInfo& b) {
        return a.key < b.key;
    });

    return out;
}

bool ConfigStore::loadFromFile(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in.is_open())
        return false;

    nlohmann::json j;
    try
    {
        in >> j;
    }
    catch (...)
    {
        return false;
    }

    // Expected structure:
    // {
    //   "ns": { "key": value, ... },
    //   ...
    // }
    if (!j.is_object())
        return false;

    m_values.clear();
    for (auto itNs = j.begin(); itNs != j.end(); ++itNs)
    {
        if (!itNs.value().is_object())
            continue;

        const std::string ns = itNs.key();
        for (auto itKey = itNs.value().begin(); itKey != itNs.value().end(); ++itKey)
        {
            const std::string key = itKey.key();
            const auto id = makeId(ns, key);
            m_values[id] = itKey.value();
        }
    }

    return true;
}

bool ConfigStore::saveToFile(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Save merged view (defaults + overrides) for registered settings,
    // plus any unknown (unregistered) keys that were loaded/added.
    nlohmann::json out = nlohmann::json::object();

    // Registered settings: use override if exists else default
    for (const auto& kv : m_registry)
    {
        const auto& info = kv.second;
        const auto id = makeId(info.ns, info.key);

        nlohmann::json value = info.defaultValue;
        auto itVal = m_values.find(id);
        if (itVal != m_values.end())
            value = itVal->second;

        out[info.ns][info.key] = value;
    }

    // Unknown values: keep them too
    for (const auto& kv : m_values)
    {
        if (m_registry.find(kv.first) != m_registry.end())
            continue;

        const auto pos = kv.first.find(':');
        if (pos == std::string::npos)
            continue;

        const std::string ns = kv.first.substr(0, pos);
        const std::string key = kv.first.substr(pos + 1);
        out[ns][key] = kv.second;
    }

    std::ofstream f(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!f.is_open())
        return false;

    f << out.dump(2);
    return true;
}

} // namespace core

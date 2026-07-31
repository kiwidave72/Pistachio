#include "SelectionManager.h"

void SelectionManager::select(const std::string& instanceId, bool additive)
{
    if (!additive) m_selectedIds.clear();
    m_selectedIds.insert(instanceId);
}

void SelectionManager::toggle(const std::string& instanceId)
{
    auto it = m_selectedIds.find(instanceId);
    if (it != m_selectedIds.end())
        m_selectedIds.erase(it);
    else
        m_selectedIds.insert(instanceId);
}

void SelectionManager::clear() { m_selectedIds.clear(); }

bool SelectionManager::isSelected(const std::string& instanceId) const
{
    return m_selectedIds.count(instanceId) > 0;
}

const std::unordered_set<std::string>& SelectionManager::getSelectedIds() const { 
    return m_selectedIds; 
}
size_t SelectionManager::count() const { return m_selectedIds.size(); }
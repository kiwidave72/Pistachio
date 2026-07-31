 
#pragma once
#include <unordered_set>
#include <string>

class SelectionManager
{
public:

    void setHovered(const std::string& instanceId) { m_hoveredId = instanceId; }
    void clearHovered() { m_hoveredId.clear(); }
    bool isHovered(const std::string& instanceId) const
    {
        return !instanceId.empty() && instanceId == m_hoveredId;
    }
    const std::string& hoveredId() const { return m_hoveredId; }
    
    void select(const std::string& instanceId, bool additive);
    void toggle(const std::string& instanceId);
    void  clear() ;
    bool isSelected(const std::string& instanceId) const;
    const std::unordered_set<std::string>& getSelectedIds() const;
    size_t count() const  ;
private:
    std::unordered_set<std::string> m_selectedIds;
     std::string m_hoveredId;   
};
#pragma once
#include <string>
#include "SelectionManager.h"  
#include "domain/dataContext.h"
#include "domain/WorkspaceStore.h"

class NavigationManager
{
public:
    void setProject(const std::string& projectId) ;
    void setBuildPlate(const std::string& plateId); 

    const std::string& currentProjectId() const;
    const std::string& currentBuildPlateId() const;
    SelectionManager& selection();

    // Resolve current context into live pointers — the only place raw pointers
    // are allowed to exist, and only ever transiently
    domain::v1::Project* resolveProject(domain::v1::Workspace* workspace) const;
    domain::v1::Project*  resolveOrDefaultProject(domain::v1::Workspace* workspace);

    domain::v1::Project* resolveProject(const domain::v1::WorkspaceStore& store) const;
    domain::v1::Project* resolveOrDefaultProject(domain::v1::WorkspaceStore& store);

    domain::v1::BuildPlate* resolveBuildPlate(domain::v1::Project* project) const;
    domain::v1::BuildPlate* resolveOrDefaultBuildPlate(domain::v1::Project* project);


private:
    std::string m_currentProjectId;
    std::string m_currentBuildPlateId;
    SelectionManager m_selection;
};
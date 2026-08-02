
#include "NavigationManager.h"
void NavigationManager::setProject(const std::string& projectId)
{
    if (m_currentProjectId.empty() || projectId != m_currentProjectId)
    {
        m_currentProjectId = projectId;
        setBuildPlate("");   // switching projects invalidates plate + selection
    }
}

void NavigationManager::setBuildPlate(const std::string& plateId)
{
    if (plateId != m_currentBuildPlateId)
    {
        m_currentBuildPlateId = plateId;
        m_selection.clear();   // instances from the old plate aren't valid anymore
    }
}

const std::string& NavigationManager::currentProjectId() const { return m_currentProjectId; }
const std::string& NavigationManager::currentBuildPlateId() const { return m_currentBuildPlateId; }
SelectionManager& NavigationManager::selection() { return m_selection; }

// Resolve current context into live pointers — the only place raw pointers
// are allowed to exist, and only ever transiently
domain::v1::Project* NavigationManager::resolveProject(domain::v1::Workspace* workspace) const
{
    if (!workspace) return nullptr;
    for (auto* p : workspace->projects)
        if (p && p->Id == m_currentProjectId) return p;
        
    return nullptr;
 }

domain::v1::Project* NavigationManager::resolveOrDefaultProject(domain::v1::Workspace* workspace) 
{
    domain::v1::Project* found = resolveProject(workspace);
    if (found) return found;

    if (workspace && !workspace->projects.empty())
    {
        setProject(workspace->projects[0]->Id);   // goes through setProject, so setBuildPlate("")/selection-clear side effects stay consistent
        return workspace->projects[0];
    }
    return nullptr;
}

domain::v1::BuildPlate* NavigationManager::resolveBuildPlate(domain::v1::Project* project) const
{
    if (!project) return nullptr;
    for (auto* bp : project->buildPlates)
        if (bp && bp->Id == m_currentBuildPlateId) return bp;
    return nullptr;
}

domain::v1::BuildPlate* NavigationManager::resolveOrDefaultBuildPlate(domain::v1::Project* project)
{
    domain::v1::BuildPlate* found = resolveBuildPlate(project);
    if (found) return found;

    if (project && !project->buildPlates.empty())
    {
        setBuildPlate(project->buildPlates[0]->Id);   // goes through setProject, so setBuildPlate("")/selection-clear side effects stay consistent
        return project->buildPlates[0];
    }
    return nullptr;
}

domain::v1::Project* NavigationManager::resolveProject(const domain::v1::WorkspaceStore& store) const
{
    domain::v1::Project* result = nullptr;
    store.read([&](const domain::v1::Workspace& ws)
        {
            for (auto* p : ws.projects)
            {
                if (p && p->Id == m_currentProjectId)
                {
                    result = p;
                    return;
                }
            }
        });
    return result;
}

domain::v1::Project* NavigationManager::resolveOrDefaultProject(domain::v1::WorkspaceStore& store)
{
    domain::v1::Project* found = resolveProject(store);
    if (found) return found;

    domain::v1::Project* firstProject = nullptr;
    store.read([&](const domain::v1::Workspace& ws)
        {
            if (!ws.projects.empty())
                firstProject = ws.projects[0];
        });

    if (firstProject)
    {
        setProject(firstProject->Id);   // goes through setProject, so setBuildPlate("")/selection-clear side effects stay consistent
        return firstProject;
    }
    return nullptr;
}
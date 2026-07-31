#include "Workspace.h"

namespace domain::v1
{

    Workspace::Workspace()
    {
        projects = std::vector<Project*>();
        repositories = std::vector<Repository*>();
    }
    Workspace::~Workspace()
    {
        for (auto p : projects) delete p;
        for (auto r : repositories) delete r;
    }

}
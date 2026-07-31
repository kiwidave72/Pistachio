#include "Project.h"

namespace domain::v1
{
	Project::Project()
	{
		buildPlates = std::vector<BuildPlate*>();
	}
	Project::~Project()
	{
		for (auto p : buildPlates) delete p;
	}
     void Project::fromScanResult(const adapters::scanning::ScanResult& scan,domain::v1::ModelCache& cache)
    {
        if (!scan.root) return;

         

        name = scan.root ? "Project [" + scan.root->name +"]" : "New Project";
        label = name;
        /*fileLocation = scan.rootPath;*/

        projectFolders.clear();
        buildPlates.clear();
        

        int nextId = 1;

          BuildPlate* plate =  new BuildPlate();
        plate->name = "Default [Empty]";
        plate->Id = 1;

        buildPlates.push_back(plate);

        projectFolders = buildFolders(*scan.root, nextId, cache);
    }


}
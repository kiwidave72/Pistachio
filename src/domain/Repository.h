#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <string>
#include <regex>
#include <sstream>
#include <fstream>

namespace domain::v1 {

    class RepositoryAsset
    {
    public:
        int         Id = 0;
        std::string name;          // clean display name (no xN/[A])
        std::string label;         // original filename stem
        std::string fileName;      // filename with extension
        std::string fileLocation;
        std::string description;   // e.g. "Print x3 | Accent colour [A]"
        int         quantity = 1;
        bool        isAccent = false;
    };

    class RepositoryFolder
    {
    public:

        RepositoryFolder();
        ~RepositoryFolder();

        int         Id = 0;
        std::string name;
        std::string label;
        std::string folderLocation;
        std::vector<RepositoryFolder*>  folders;

        std::vector<RepositoryAsset*>  importedAssets;

       /* std::vector<ImportedAsset>  importedAssets;
        std::vector<RepositoryFolder>  folders;*/
        int totalAssetCount();
       
    };

	class Repository
	{
    public:
	 
        Repository();
        ~Repository();
		std::string name;
		std::string label;
		std::string remoteUrl;
		std::string branch;

		//add other git related metadata here like remote URL, branch, last commit hash etc. if needed for future features like syncing projects with git repositories.
		std::vector<RepositoryFolder*> folders;
	};


}
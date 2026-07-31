#pragma once
#include <vector>
#include <string>

#include "domain\ModelCache.h"
namespace domain::v1 {

     class ProjectFolder
    {
    public:
        int         Id = 0;
        std::string name;
        std::string label;
        std::string folderLocation;
        std::vector<std::unique_ptr<ImportedAsset>> importedAssets;
        std::vector<domain::v1::ProjectFolder>  folders;

        int totalAssetCount() const
        {
            int n = (int)importedAssets.size();
            for (const auto& f : folders) n += f.totalAssetCount();
            return n;
        }
    };

}

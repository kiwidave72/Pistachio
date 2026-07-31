#pragma once
#include <string>
#include <vector>
#include "BuildPlate.h"
#include <nlohmann/json.hpp> // Include the nlohmann/json library

#include "ProjectFolder.h"
#include "C:\github\Pistachio-config\src\adapters\ui\plugins\SlicerCorePlugin\core\FolderScanner.h"

namespace domain::v1
{
    // -----------------------------------------------------------------------
    // ParsedFileMeta
    // Extracted from filename conventions:
    //   [A]     = accent colour part
    //   x2/x3/x4 = quantity to print
    //
    // Examples:
    //   body_x2.stl          -> quantity=2, accent=false
    //   clip_[A]_x3.stl      -> quantity=3, accent=true
    //   mount_[A].stl        -> quantity=1, accent=true
    //   frame.stl            -> quantity=1, accent=false
    // -----------------------------------------------------------------------
    struct ParsedFileMeta
    {
        std::string cleanName;   // name with [A] and xN stripped
        int         quantity = 1;
        bool        isAccent = false;

        static ParsedFileMeta parse(const std::string& stem)
        {
            ParsedFileMeta meta;
            std::string s = stem;

            // Detect [A] accent marker (case insensitive)
            {
                std::regex accentRe(R"(\[A\])", std::regex::icase);
                if (std::regex_search(s, accentRe))
                {
                    meta.isAccent = true;
                    s = std::regex_replace(s, accentRe, "");
                }
            }

            // Detect xN quantity marker e.g. _x2, _x3, x4 (with or without underscore)
            {
                std::regex qtyRe(R"(_?[xX](\d+))");
                std::smatch m;
                if (std::regex_search(s, m, qtyRe))
                {
                    meta.quantity = std::stoi(m[1].str());
                    s = std::regex_replace(s, qtyRe, "");
                }
            }

            // Clean up trailing/leading underscores and spaces
            while (!s.empty() && (s.front() == '_' || s.front() == ' ')) s.erase(s.begin());
            while (!s.empty() && (s.back() == '_' || s.back() == ' ')) s.pop_back();

            meta.cleanName = s.empty() ? stem : s;
            return meta;
        }

        std::string buildDescription() const
        {
            std::ostringstream oss;
            if (quantity > 1)
                oss << "Print x" << quantity;
            if (isAccent)
            {
                if (quantity > 1) oss << " | ";
                oss << "Accent colour [A]";
            }
            return oss.str();
        }
    };

    class Project
    {
    public:
        Project();
        ~Project();
        std::string Id;
        std::string name;
        std::string label;
       
		std::vector<domain::v1::BuildPlate*> buildPlates;
        std::vector<domain::v1::ProjectFolder> projectFolders;
    
        domain::v1::ModelCache m_cache;
        
        void Project::fromScanResult(const adapters::scanning::ScanResult& scan, domain::v1::ModelCache& cache);


        static std::vector<ProjectFolder> buildFolders(
            const adapters::scanning::ScanFolder& scanFolder,
            int& nextId, domain::v1::ModelCache& cache)
        {
            std::vector<ProjectFolder> result;

            // One ProjectFolder per ScanFolder that has assets
            ProjectFolder pf;
            pf.Id = nextId++;
            pf.name = scanFolder.name;
            pf.label = scanFolder.name;
            pf.folderLocation = scanFolder.fullPath;

            // Convert files to ImportedAssets
            for (const auto& file : scanFolder.files)
            {
                auto meta = ParsedFileMeta::parse(file.name);

                 auto asset = std::make_unique<ImportedAsset>();

                asset->Id = file.fileHash.c_str();// nextId++;
                asset->name = meta.cleanName;
                asset->label = file.name;          // original stem
                asset->fileName = file.filename;
                asset->fileLocation = file.fullPath;
                asset->quantity = meta.quantity;
                asset->isAccent = meta.isAccent;
                asset->description = meta.buildDescription();

                cache.assets[asset->Id] = asset.get() ;

                pf.importedAssets.push_back(std::move(asset));
            }

            // Recurse into subfolders
            for (const auto& sub : scanFolder.subFolders) 
            {
                auto subFolders = buildFolders(*sub, nextId,cache);
                for (auto& sf : subFolders)
                    pf.folders.push_back(std::move(sf));
            }

            // Only add folder if it has content
            if (!pf.importedAssets.empty() || !pf.folders.empty())
                result.push_back(std::move(pf));

            return result;
        }
    };


    // 1. Convert Project instance to JSON
    inline void to_json(nlohmann::json& j, const Project& p) {
        j["Id"] = p.Id;
        j["name"] = p.name;
        j["label"] = p.label;

        // Serialize buildPlates pointer array safely
        j["buildPlates"] = nlohmann::json::array();
        for (const auto* plate : p.buildPlates) {
            if (plate) {
                j["buildPlates"].push_back(*plate); // Dereferences and calls BuildPlate's serializer
            }
            else {
                j["buildPlates"].push_back(nullptr);
            }
        }
    }

    // 2. Convert JSON back into a Project instance
    inline void from_json(const nlohmann::json& j, Project& p) {
        // Clear out any old pointer elements to prevent memory leaks if overwriting
        for (auto* plate : p.buildPlates) delete plate;
        p.buildPlates.clear();

        // Deserialize standard fields safely
        if (j.contains("Id") && j["Id"].is_string()) {
            j["Id"].get_to(p.Id);
        }
        if (j.contains("name") && j["name"].is_string()) {
            j["name"].get_to(p.name);
        }
        if (j.contains("label") && j["label"].is_string()) {
            j["label"].get_to(p.label);
        }

        // Deserialize buildPlates pointer array safely
        if (j.contains("buildPlates") && j["buildPlates"].is_array()) {
            for (const auto& element : j["buildPlates"]) {
                if (element.is_null()) {
                    p.buildPlates.push_back(nullptr);
                }
                else {
                    BuildPlate* plate = new BuildPlate();
                    element.get_to(*plate); // Assumes BuildPlate has its own to_json/from_json
                    p.buildPlates.push_back(plate);
                }
            }
        }
    }
    

}
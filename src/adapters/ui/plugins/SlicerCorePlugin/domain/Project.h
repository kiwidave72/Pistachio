//#pragma once
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/type_ptr.hpp>
//
// 
//#include "BoundaryBox.h"
//#include <vector>
//#include <string>
//#include <regex>
//#include <sstream>
//#include <fstream>
// 
//#include "..\\core\\FolderScanner.h"
//
//
//
//using namespace adapters::scanning ;
//
//namespace slicer::core
//{
//
//
//
//
//// -----------------------------------------------------------------------
//// Domain classes
//// -----------------------------------------------------------------------
// 
//class Vertex
//{
//public:
//    glm::vec3 position;
//    glm::vec3 normal;
//};
//
//class Triangle
//{
//public:
//    glm::vec3 normal;
//    glm::vec3 v1;
//    glm::vec3 v2;
//    glm::vec3 v3;
//};
//
//struct TriangleView
//{
//    const Vertex* v1;
//    const Vertex* v2;
//    const Vertex* v3;
//};
//
//
//
//// Mesh is the stl that is loaded , and may be repaired or optimised to reduce duplication of indexed vertices. 
//// It is stored in the MeshCache and referenced by Models on the build plate. 
//// It is separate from Model because the same mesh may be instanced multiple times with different transforms and quantities.
//class Mesh
//{
//public:
//
//    int Id = 0;
//
//    std::string name;
//    std::string fileName;
//    std::string fileLocation;
//
//    std::vector<Vertex> vertices;
//    std::vector<uint32_t> indices;
//
//    BoundingBox bounds;
//
//    glm::vec3 center;
//
//    float cameradistance = 0.0f;
//    float radius = 0.0f;
//
//    TriangleView Mesh::triangle(size_t index) const
//    {
//        size_t i = index * 3;
//
//        return
//        {
//            &vertices[indices[i]],
//            &vertices[indices[i + 1]],
//            &vertices[indices[i + 2]]
//        };
//    }
//};
//
////openGL buffers for rendering a mesh. Stored separately from Mesh so we can keep Mesh as pure data and only create GL resources when needed for rendering.
//class MeshRenderData
//{
//public:
//
//    unsigned int vao = 0;
//    unsigned int vbo = 0;
//    unsigned int ebo = 0;
//
//    int vertexCount = 0;
//    int indexCount = 0;
//};
//
//
//
////// ImportedAsset represents a single STL file found during the scan, along with metadata parsed from the filename (quantity, accent marker) and a clean display name.
//class ImportedAsset
//{
//public:
//    std::string         Id = "";
//    std::string name;          // clean display name (no xN/[A])
//    std::string label;         // original filename stem
//    std::string fileName;      // filename with extension
//    std::string fileLocation;
//    std::string description;   // e.g. "Print x3 | Accent colour [A]"
//    int         quantity = 1;
//    bool        isAccent = false;
//};
//
//// MeshCache stores loaded meshes keyed by file location. When an asset is added to the build plate, its mesh is loaded and stored here. 
//// This allows sharing meshes between multiple instances of the same asset and potentially saving/loading preprocessed meshes in future for faster load times.
//class MeshCache
//{
//public:
//    std::unordered_map<std::string, ImportedAsset*> assets;
//    std::unordered_map<std::string, std::shared_ptr<Mesh>> meshes;
// };
//
//
//
//
//// Model represents an instance of an asset placed on the build plate, with transform and quantity. It references the mesh via the importedAssetId and can have its own position/rotation/scale.
//
//class Model
//{
//public:
//    int         Id = 0;
//    std::string name;
//    std::string label;
//    std::string fileName;
//    std::string fileLocation;
//    int         importedAssetId = 0; // optional reference to ImportedAsset
//    int         quantity = 1;
//    bool        isAccent = false;
//	bool		selected = false;
//
//    glm::vec3 color{ 0.65f, 0.35f, 0.85f }; // Default purple
//
//    glm::vec3 worldPosition;
//    glm::vec3 localPosition;
//    glm::vec3 rotation;
//    glm::vec3 scale{ 1,1,1 };
//
//    //glm::vec3 meshCenter;
//
//    std::shared_ptr<Mesh> m_mesh;
//
//    BoundingBox localBounds;
//    BoundingBox worldBounds;
//};
//
// 
//
//
//// PrinterProfileSettings represents the printer's build volume and spacing requirements. This can be used to validate model placements on the build plate and provide feedback to the user.
//// not sure how to store these to the file system yet, we populate thim from the config store for now and may want to add UI for editing them in future. and into a different file for sharing and reusing between projects.
//class PrinterProfileSettings
//{
//public:
//    int Id = 0;
//    int X = 0;
//    int Y = 0;
//    int Z = 0;
//
//	float partSpacing = 3.0f; // mm between parts on the build plate
//	float buildplateMargin = 3.0f; // mm margin from build plate edges where parts cannot be placed
//
//};
//
//// PrinterProfile represents a named printer configuration with its settings. This allows users to select different printer profiles for their projects and ensures that model placements are validated against the selected printer's capabilities.
//
//class PrinterProfile
//{
//public:
//    int         Id = 0;
//    std::string name;
//    std::string label;
//    std::string filename;
//    std::string fileLocation;
//    PrinterProfileSettings settings;
//
//};
//
////BuildPlate represents the virtual build plate in the slicer where models are placed for slicing. It contains a list of models and references the selected printer profile settings for validation.
//class BuildPlate
//{
//public:
//    int         Id = 0;
//    std::string name;
//    std::string label;
//    std::vector<std::shared_ptr<Model>> models;
//
//	PrinterProfileSettings* profileSettings = nullptr;
//
//    std::shared_ptr<slicer::core::Model> buildPlateModel;
//};
//
//// Project is the top-level domain object representing the user's current project. It contains the project folders (mirroring the scanned folder structure), the build plates with placed models, and a cache of loaded meshes for efficient rendering and slicing.
//class Project
//{
//public:
//    std::string name;
//    std::string label;
//    std::string fileName;
//    std::string fileLocation;
//    std::vector<std::shared_ptr<BuildPlate>>    buildPlates;
//    //std::vector<domain::v1::ProjectFolder> projectFolders;
//    //MeshCache meshCache; //  when an asset is loaded added to the build plate , its mesh is stored here keyed by fileLocation as this. may want to save this to disk in future to speed up load times
//
//	PrinterProfile* selectedProfile = nullptr;
//    
//    // -----------------------------------------------------------------------
//    // Factory — build a Project from a FolderScanner result.
//    //
//    // Each ScanFolder becomes a ProjectFolder.
//    // Each ScanFile becomes an ImportedAsset with quantity/accent parsed
//    // from the filename.
//    // -----------------------------------------------------------------------
//   /* void fromScanResult(const adapters::scanning::ScanResult& scan)
//    {
//        if (!scan.root) return;
//
//        name = scan.root ? "Project [" + scan.root->name +"]" : "New Project";
//        label = name;
//        fileLocation = scan.rootPath;
//
//        projectFolders.clear();
//        buildPlates.clear();
//
//
//        int nextId = 1;
//
//        buildPlates = std::vector<std::shared_ptr<BuildPlate>>();
//        std::shared_ptr<BuildPlate> plate = std::make_shared<BuildPlate>();
//        plate->name = "Default [Empty]";
//        plate->Id = 1;
//
//        buildPlates.push_back(plate);
//
//        projectFolders = buildFolders(*scan.root, nextId);
//    }*/
//
//private:
//    //static std::vector<ProjectFolder> buildFolders(
//    //    const adapters::scanning::ScanFolder& scanFolder,
//    //    int& nextId)
//    //{
//    //    std::vector<ProjectFolder> result;
//
//    //    // One ProjectFolder per ScanFolder that has assets
//    //    ProjectFolder pf;
//    //    pf.Id = nextId++;
//    //    pf.name = scanFolder.name;
//    //    pf.label = scanFolder.name;
//    //    pf.folderLocation = scanFolder.fullPath;
//
//    //    // Convert files to ImportedAssets
//    //    for (const auto& file : scanFolder.files)
//    //    {
//    //        auto meta = ParsedFileMeta::parse(file.name);
//
//    //        ImportedAsset asset;
//    //        asset.Id = file.fileHash.c_str() ;// nextId++;
//    //        asset.name = meta.cleanName;
//    //        asset.label = file.name;          // original stem
//    //        asset.fileName = file.filename;
//    //        asset.fileLocation = file.fullPath;
//    //        asset.quantity = meta.quantity;
//    //        asset.isAccent = meta.isAccent;
//    //        asset.description = meta.buildDescription();
//
//    //        pf.importedAssets.push_back(std::move(asset));
//    //    }
//
//    //    // Recurse into subfolders
//    //    for (const auto& sub : scanFolder.subFolders)
//    //    {
//    //        auto subFolders = buildFolders(*sub, nextId);
//    //        for (auto& sf : subFolders)
//    //            pf.folders.push_back(std::move(sf));
//    //    }
//
//    //    // Only add folder if it has content
//    //    if (!pf.importedAssets.empty() || !pf.folders.empty())
//    //        result.push_back(std::move(pf));
//
//    //    return result;
//    //}
//};
//
////class Repository
////{
////	int Id = 0;
////	std::string name;
////	std::string label;
////	std::string remoteUrl;
////	std::string branch;
////
////	//add other git related metadata here like remote URL, branch, last commit hash etc. if needed for future features like syncing projects with git repositories.
////    std::vector<ProjectFolder> folder;
////};
//
//
//  
//
//struct RawTriangle
//{
//    glm::vec3 normal;
//    glm::vec3 v[3];
//};
//
////class StlLoaderAdapter 
////{
////public:
////
////    explicit StlLoaderAdapter(MeshCache& cache);
////    
////    std::vector<RawTriangle> Triangle();
////    std::shared_ptr<slicer::core::Mesh> getMesh() ;
////    bool load(const std::string& filePath);
////
////
////private:
////    std::vector<RawTriangle> tris;
////    std::shared_ptr<slicer::core::Mesh> m_mesh;
////    
////    MeshCache& m_cache;
////
////    bool loadBinaryStl(const std::string& path, std::vector<RawTriangle>& out)
////    {
////        std::ifstream f(path, std::ios::binary);
////        if (!f) return false;
////
////        char header[80];
////        f.read(header, 80);
////        if (!f) return false;
////
////        uint32_t triCount = 0;
////        f.read(reinterpret_cast<char*>(&triCount), 4);
////        if (!f || triCount == 0) return false;
////
////        out.reserve(triCount);
////        for (uint32_t i = 0; i < triCount; ++i)
////        {
////            RawTriangle t;
////            float buf[12];
////            f.read(reinterpret_cast<char*>(buf), 48);
////            uint16_t attr;
////            f.read(reinterpret_cast<char*>(&attr), 2);
////            if (!f) break;
////
////            t.normal = { buf[0], buf[1], buf[2] };
////            t.v[0] = { buf[3], buf[4], buf[5] };
////            t.v[1] = { buf[6], buf[7], buf[8] };
////            t.v[2] = { buf[9], buf[10], buf[11] };
////            out.push_back(t);
////        }
////        return !out.empty();
////    }
////
////    bool isAsciiStl(const std::string& path)
////    {
////        std::ifstream f(path);
////        std::string word;
////        f >> word;
////        return (word == "solid");
////    }
////
////    bool loadAsciiStl(const std::string& path, std::vector<RawTriangle>& out)
////    {
////        std::ifstream f(path);
////        if (!f) return false;
////
////        std::string tok;
////        RawTriangle tri{};
////        int vertIdx = 0;
////        bool inFacet = false;
////
////        while (f >> tok)
////        {
////            if (tok == "facet")
////            {
////                std::string norm; float nx, ny, nz;
////                f >> norm >> nx >> ny >> nz;
////                tri.normal = { nx,ny,nz };
////                vertIdx = 0;
////                inFacet = true;
////            }
////            else if (tok == "vertex" && inFacet && vertIdx < 3)
////            {
////                float x, y, z;
////                f >> x >> y >> z;
////                tri.v[vertIdx++] = { x,y,z };
////            }
////            else if (tok == "endfacet" && inFacet)
////            {
////                if (vertIdx == 3) out.push_back(tri);
////                inFacet = false;
////            }
////        }
////        return !out.empty();
////    }
////
////
////};
//
//
//
//}
//
//

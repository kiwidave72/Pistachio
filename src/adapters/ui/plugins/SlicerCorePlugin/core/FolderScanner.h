#pragma once

// -----------------------------------------------------------------------
// FolderScanner.h
//
// Scans a root folder for STL files and builds a navigable hierarchy of
// folders and files. Designed to feed into a slicer project builder.
//
// Usage:
//   FolderScanner scanner;
//   auto root = scanner.scan("C:/Models");
//   // root is a shared_ptr<ScanFolder> representing the root
//
// Place at: src/adapters/scanning/FolderScanner.h
// -----------------------------------------------------------------------

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <functional>

namespace adapters::scanning {

    // -----------------------------------------------------------------------
    // ScanFile � represents a single STL file found during the scan
    // -----------------------------------------------------------------------
    struct ScanFile
    {
        std::string name;         // filename without extension e.g. "benchy"
        std::string filename;     // filename with extension   e.g. "benchy.stl"
        std::string fullPath;     // absolute path             e.g. "C:/Models/Boats/benchy.stl"
        uintmax_t   fileSizeBytes = 0;
        std::string fileHash;

        // Convenience
        std::string extension() const
        {
            auto p = std::filesystem::path(filename).extension().string();
            // lowercase
            for (auto& c : p) c = (char)std::tolower((unsigned char)c);
            return p;
        }


    };

    // -----------------------------------------------------------------------
    // ScanFolder � represents a folder node in the scanned hierarchy
    // Only folders that contain at least one STL file (directly or in a
    // descendant) are included in the tree.
    // -----------------------------------------------------------------------
    struct ScanFolder
    {
        std::string name;         // folder display name   e.g. "Boats"
        std::string fullPath;     // absolute path         e.g. "C:/Models/Boats"

        std::vector<ScanFile>                       files;       // STL files directly in this folder
        std::vector<std::shared_ptr<ScanFolder>>    subFolders;  // child folders that have STL content

        // Helpers
        bool hasFiles()      const { return !files.empty(); }
        bool hasSubFolders() const { return !subFolders.empty(); }
        bool isEmpty()       const { return files.empty() && subFolders.empty(); }

        int totalFileCount() const
        {
            int n = (int)files.size();
            for (const auto& sub : subFolders)
                n += sub->totalFileCount();
            return n;
        }
    };

    // -----------------------------------------------------------------------
    // ScanFileResult � result of a single-file scan
    // -----------------------------------------------------------------------
    struct ScanFileResult
    {
        ScanFile    file;
        std::string errorMessage;
        bool        success = false;
    };

    // -----------------------------------------------------------------------
    // ScanResult � top-level result returned by FolderScanner::scan()
    // -----------------------------------------------------------------------
    struct ScanResult
    {
        std::shared_ptr<ScanFolder> root;        // root folder node
        int                         totalFiles = 0;
        int                         totalFolders = 0;
        std::string                 rootPath;    // the path that was scanned
        std::string                 errorMessage;
        bool                        success = false;

        // Flat list of all files found � convenient for building a project
        std::vector<const ScanFile*> allFiles() const
        {
            std::vector<const ScanFile*> out;
            if (root) collectFiles(root, out);
            return out;
        }

    private:
        static void collectFiles(
            const std::shared_ptr<ScanFolder>& folder,
            std::vector<const ScanFile*>& out)
        {
            for (const auto& f : folder->files)
                out.push_back(&f);
            for (const auto& sub : folder->subFolders)
                collectFiles(sub, out);
        }
    };

    // -----------------------------------------------------------------------
    // ScanOptions � controls scan behaviour
    // -----------------------------------------------------------------------
    struct ScanOptions
    {
        bool        recursive = true;    // scan subdirectories
        bool        caseSensitive = false;   // extension matching
        bool        includeHidden = false;   // skip hidden files/folders
        int         maxDepth = 32;      // max recursion depth
        uintmax_t   minFileSizeBytes = 0;       // skip files smaller than this
        uintmax_t   maxFileSizeBytes = 0;       // 0 = no limit

        // Extra extensions to include alongside .stl (e.g. ".3mf", ".obj")
        std::vector<std::string> extraExtensions;

        // Optional progress callback � called for each folder entered
        // Return false to cancel the scan
        std::function<bool(const std::string& currentPath, int filesFound)> onProgress;

        // Optional hash shortcut - called before hashFile() for every candidate file.
        // Args: full path, current size (bytes), current last-write-time (epoch count).
        // Return a non-empty string to use it as the hash and skip re-hashing;
        // return an empty string to force FolderScanner to hash the file normally.
        // Lets a caching layer skip re-hashing files whose size+mtime are unchanged.
        std::function<std::string(const std::filesystem::path& p,
            uintmax_t sizeBytes,
            int64_t   lastWriteTime)> hashLookup;
    };

    // -----------------------------------------------------------------------
    // FolderScanner
    // -----------------------------------------------------------------------
    class FolderScanner
    {
    public:
        FolderScanner() = default;

        // Scan rootPath and return a ScanResult.
        // Thread-safe � each call is independent.
        ScanResult scan(const std::string& rootPath,
            const ScanOptions& options = {}) const;

        ScanFileResult scanFile(const std::string& filePath,
            const ScanOptions& options = {}) const;

    private:
        // Recursively scan a directory � returns nullptr if no STL content found
        std::shared_ptr<ScanFolder> scanDirectory(
            const std::filesystem::path& dirPath,
            const ScanOptions& options,
            int                          depth,
            int& totalFiles,
            int& totalFolders,
            bool& cancelled) const;

        bool isStlFile(const std::filesystem::path& p,
            const ScanOptions& options) const;

        bool isHidden(const std::filesystem::path& p) const;

        std::string hashFile(const std::filesystem::path& p) const;

        static std::string toDisplayName(const std::filesystem::path& p)
        {
            return p.filename().string();
        }
    };

} // namespace adapters::scanning
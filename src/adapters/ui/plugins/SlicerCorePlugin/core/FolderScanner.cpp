#include "FolderScanner.h"
#include <fstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace adapters::scanning {

    // -----------------------------------------------------------------------
    // Public entry point
    // -----------------------------------------------------------------------

    ScanFileResult FolderScanner::scanFile(const std::string& filePath,
        const ScanOptions& options) const
    {
        ScanFileResult result;
        fs::path p(filePath);

        if (!fs::exists(p))
        {
            result.errorMessage = "File does not exist: " + filePath;
            return result;
        }

        if (!fs::is_regular_file(p))
        {
            result.errorMessage = "Path is not a regular file: " + filePath;
            return result;
        }

        if (!isStlFile(p, options))
        {
            result.errorMessage = "File is not a recognised STL/model file: " + filePath;
            return result;
        }

        std::error_code ec;
        uintmax_t sz = fs::file_size(p, ec);
        if (ec) sz = 0;

        result.file.filename = p.filename().string();
        result.file.name = p.stem().string();
        result.file.fullPath = fs::absolute(p).string();
        result.file.fileSizeBytes = sz;
        result.file.fileHash = hashFile(p);
        result.success = true;

        return result;
    }

    ScanResult FolderScanner::scan(const std::string& rootPath,
        const ScanOptions& options) const
    {
        ScanResult result;
        result.rootPath = rootPath;

        fs::path root(rootPath);

        if (!fs::exists(root))
        {
            result.errorMessage = "Path does not exist: " + rootPath;
            result.success = false;
            return result;
        }

        if (!fs::is_directory(root))
        {
            result.errorMessage = "Path is not a directory: " + rootPath;
            result.success = false;
            return result;
        }

        bool cancelled = false;

        result.root = scanDirectory(
            root, options, 0,
            result.totalFiles, result.totalFolders,
            cancelled);

        if (cancelled)
        {
            result.errorMessage = "Scan cancelled by progress callback";
            result.success = false;
            return result;
        }

        // If root had no STL content, still return an empty root node
        if (!result.root)
        {
            result.root = std::make_shared<ScanFolder>();
            result.root->name = toDisplayName(root);
            result.root->fullPath = fs::absolute(root).string();
        }

        result.success = true;
        return result;
    }

    // -----------------------------------------------------------------------
    // Recursive directory scan
    // -----------------------------------------------------------------------

    std::shared_ptr<ScanFolder> FolderScanner::scanDirectory(
        const fs::path& dirPath,
        const ScanOptions& options,
        int                depth,
        int& totalFiles,
        int& totalFolders,
        bool& cancelled) const
    {
        if (cancelled) return nullptr;
        if (depth > options.maxDepth) return nullptr;

        // Progress callback
        if (options.onProgress)
        {
            if (!options.onProgress(dirPath.string(), totalFiles))
            {
                cancelled = true;
                return nullptr;
            }
        }

        auto folder = std::make_shared<ScanFolder>();
        folder->name = toDisplayName(dirPath);
        folder->fullPath = fs::absolute(dirPath).string();

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dirPath, ec))
        {
            if (ec) break;
            if (cancelled) break;

            const fs::path& p = entry.path();

            // Skip hidden entries unless requested
            if (!options.includeHidden && isHidden(p))
                continue;

            if (fs::is_regular_file(p, ec))
            {
                if (!isStlFile(p, options)) continue;

                // Size filter
                uintmax_t sz = fs::file_size(p, ec);
                if (ec) sz = 0;

                if (options.minFileSizeBytes > 0 && sz < options.minFileSizeBytes)
                    continue;
                if (options.maxFileSizeBytes > 0 && sz > options.maxFileSizeBytes)
                    continue;

                ScanFile file;
                file.filename = p.filename().string();
                file.name = p.stem().string();
                file.fullPath = fs::absolute(p).string();
                file.fileSizeBytes = sz;

                // Ask the caller if it already knows this file's hash (unchanged
                // since last scan). Falls back to a real hash if hashLookup is
                // unset or returns empty (new/modified file).
                std::string cachedHash;
                if (options.hashLookup)
                {
                    auto mtime = fs::last_write_time(p, ec);
                    if (!ec)
                        cachedHash = options.hashLookup(p, sz, mtime.time_since_epoch().count());
                }
                file.fileHash = !cachedHash.empty() ? cachedHash : hashFile(p);

                folder->files.push_back(std::move(file));
                ++totalFiles;
            }
            else if (options.recursive && fs::is_directory(p, ec))
            {
                auto sub = scanDirectory(
                    p, options, depth + 1,
                    totalFiles, totalFolders, cancelled);

                // Only include subfolder if it has STL content
                if (sub && !sub->isEmpty())
                {
                    folder->subFolders.push_back(std::move(sub));
                    ++totalFolders;
                }
            }
        }

        // Sort files alphabetically by name
        std::sort(folder->files.begin(), folder->files.end(),
            [](const ScanFile& a, const ScanFile& b) {
                return a.name < b.name;
            });

        // Sort subfolders alphabetically
        std::sort(folder->subFolders.begin(), folder->subFolders.end(),
            [](const std::shared_ptr<ScanFolder>& a,
                const std::shared_ptr<ScanFolder>& b) {
                    return a->name < b->name;
            });

        return folder;
    }

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    bool FolderScanner::isStlFile(const fs::path& p,
        const ScanOptions& options) const
    {
        std::string ext = p.extension().string();

        // Normalise to lowercase for comparison
        std::string extLower = ext;
        std::transform(extLower.begin(), extLower.end(), extLower.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });

        if (extLower == ".stl") return true;

        // Extra extensions
        for (const auto& extra : options.extraExtensions)
        {
            std::string extraLower = extra;
            std::transform(extraLower.begin(), extraLower.end(), extraLower.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });

            if (options.caseSensitive ? (ext == extra) : (extLower == extraLower))
                return true;
        }

        return false;
    }

    bool FolderScanner::isHidden(const fs::path& p) const
    {
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(p.string().c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) return false;
        return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
        // On Unix, hidden = starts with '.'
        return p.filename().string().starts_with('.');
#endif
    }

    std::string FolderScanner::hashFile(const fs::path& p) const
    {
        constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
        constexpr uint64_t FNV_PRIME = 1099511628211ULL;

        std::ifstream f(p, std::ios::binary);
        if (!f) return {};

        uint64_t hash = FNV_OFFSET;
        char buf[65536];
        while (f.read(buf, sizeof(buf)) || f.gcount() > 0)
        {
            for (std::streamsize i = 0; i < f.gcount(); ++i)
            {
                hash ^= static_cast<uint8_t>(buf[i]);
                hash *= FNV_PRIME;
            }
        }

        // Format as 16-char zero-padded hex string
        char hex[17];
        snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(hash));
        return std::string(hex);
    }

} // namespace adapters::scanning
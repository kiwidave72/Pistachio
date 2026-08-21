#pragma once
#include "FolderScanner.h"
#include <fstream>
#include <filesystem>
#include <unordered_map>

namespace adapters::scanning {

    class CachedFolderScanner
    {
    public:
        // fresh=true forces a full rehash, ignoring the fingerprint cache
        ScanResult scan(const std::string& rootPath, ScanOptions options,
            const std::string& cacheDir = ".cache", bool fresh = false)
        {
            std::filesystem::create_directories(cacheDir);
            std::string cacheFile = cacheDir + "/" + hashPath(rootPath) + ".scan";

            std::unordered_map<std::string, FileFingerprint> prior;
            if (!fresh) loadFingerprints(cacheFile, prior);

            // Wrap any hashLookup the caller already set, but default to the
            // fingerprint-cache shortcut.
            auto userLookup = options.hashLookup;
            options.hashLookup = [&prior, userLookup](const std::filesystem::path& p,
                uintmax_t size, int64_t mtime) -> std::string
                {
                    if (userLookup) {
                        auto h = userLookup(p, size, mtime);
                        if (!h.empty()) return h;
                    }
                    auto it = prior.find(p.string());
                    if (it != prior.end() && it->second.size == size && it->second.mtime == mtime)
                        return it->second.hash;
                    return {};
                };

            ScanResult result = scanner_.scan(rootPath, options);
            if (result.success) saveCache(cacheFile, result);
            return result;
        }

    private:
        struct FileFingerprint { uintmax_t size; int64_t mtime; std::string hash; };

        FolderScanner scanner_;

        static std::string hashPath(const std::string& path)
        {
            uint64_t h = 14695981039346656037ULL;
            for (char c : path) { h ^= (uint8_t)c; h *= 1099511628211ULL; }
            char buf[17]; snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
            return buf;
        }

        // ---- binary format ----
        // [u32 magic][u32 version][string rootPath][i32 totalFiles][i32 totalFolders][folder tree]
        static void writeStr(std::ofstream& f, const std::string& s)
        {
            uint32_t len = (uint32_t)s.size();
            f.write((char*)&len, sizeof(len));
            f.write(s.data(), len);
        }
        static std::string readStr(std::ifstream& f)
        {
            uint32_t len = 0;
            f.read((char*)&len, sizeof(len));
            std::string s(len, '\0');
            f.read(s.data(), len);
            return s;
        }

        static void writeFile(std::ofstream& f, const ScanFile& sf)
        {
            writeStr(f, sf.name);
            writeStr(f, sf.filename);
            writeStr(f, sf.fullPath);
            f.write((char*)&sf.fileSizeBytes, sizeof(sf.fileSizeBytes));
            writeStr(f, sf.fileHash);
        }
        static ScanFile readFile(std::ifstream& f)
        {
            ScanFile sf;
            sf.name = readStr(f);
            sf.filename = readStr(f);
            sf.fullPath = readStr(f);
            f.read((char*)&sf.fileSizeBytes, sizeof(sf.fileSizeBytes));
            sf.fileHash = readStr(f);
            return sf;
        }

        static void writeFolder(std::ofstream& f, const std::shared_ptr<ScanFolder>& folder)
        {
            writeStr(f, folder->name);
            writeStr(f, folder->fullPath);

            uint32_t fileCount = (uint32_t)folder->files.size();
            f.write((char*)&fileCount, sizeof(fileCount));
            for (auto& sf : folder->files) writeFile(f, sf);

            uint32_t subCount = (uint32_t)folder->subFolders.size();
            f.write((char*)&subCount, sizeof(subCount));
            for (auto& sub : folder->subFolders) writeFolder(f, sub);
        }
        static std::shared_ptr<ScanFolder> readFolder(std::ifstream& f)
        {
            auto folder = std::make_shared<ScanFolder>();
            folder->name = readStr(f);
            folder->fullPath = readStr(f);

            uint32_t fileCount = 0;
            f.read((char*)&fileCount, sizeof(fileCount));
            folder->files.reserve(fileCount);
            for (uint32_t i = 0; i < fileCount; ++i) folder->files.push_back(readFile(f));

            uint32_t subCount = 0;
            f.read((char*)&subCount, sizeof(subCount));
            folder->subFolders.reserve(subCount);
            for (uint32_t i = 0; i < subCount; ++i) folder->subFolders.push_back(readFolder(f));

            return folder;
        }

        static constexpr uint32_t kMagic = 0x53434E31; // "SCN1"
        static constexpr uint32_t kVersion = 1;

        static void saveCache(const std::string& path, const ScanResult& r)
        {
            std::ofstream f(path, std::ios::binary);
            if (!f || !r.root) return;
            f.write((char*)&kMagic, sizeof(kMagic));
            f.write((char*)&kVersion, sizeof(kVersion));
            writeStr(f, r.rootPath);
            f.write((char*)&r.totalFiles, sizeof(r.totalFiles));
            f.write((char*)&r.totalFolders, sizeof(r.totalFolders));
            writeFolder(f, r.root);
        }

        bool loadCache(const std::string& path, ScanResult& out)
        {
            std::ifstream f(path, std::ios::binary);
            if (!f) return false;
            uint32_t magic = 0, version = 0;
            f.read((char*)&magic, sizeof(magic));
            f.read((char*)&version, sizeof(version));
            if (magic != kMagic || version != kVersion) return false;

            out.rootPath = readStr(f);
            f.read((char*)&out.totalFiles, sizeof(out.totalFiles));
            f.read((char*)&out.totalFolders, sizeof(out.totalFolders));
            out.root = readFolder(f);
            out.success = true;
            return true;
        }

        void loadFingerprints(const std::string& cacheFile,
            std::unordered_map<std::string, FileFingerprint>& out)
        {
            ScanResult cached;
            if (!loadCache(cacheFile, cached) || !cached.root) return;

            std::function<void(const std::shared_ptr<ScanFolder>&)> walk =
                [&](const std::shared_ptr<ScanFolder>& folder) {
                for (auto& sf : folder->files) {
                    std::error_code ec;
                    auto mtime = std::filesystem::last_write_time(sf.fullPath, ec);
                    if (ec) continue; // file gone/moved since cache was written
                    out[sf.fullPath] = { sf.fileSizeBytes,
                                          mtime.time_since_epoch().count(),
                                          sf.fileHash };
                }
                for (auto& sub : folder->subFolders) walk(sub);
                };
            walk(cached.root);
        }
    };

} // namespace adapters::scanning
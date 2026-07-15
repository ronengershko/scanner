#pragma once
#include <cstdint>
#include <filesystem>

struct FileMetadata {
    std::filesystem::path normalizedPath;
    uint64_t deviceId;
    uint64_t inode;
    uintmax_t size;
    int64_t modificationTime;
};

class FileMetadataProvider {
public:
    FileMetadata read(const std::filesystem::path& path) const;
    bool sameFileState(const FileMetadata& first, const FileMetadata& second) const;
};

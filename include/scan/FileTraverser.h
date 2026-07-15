#pragma once
#include <filesystem>
#include <functional>

using FileCallback = std::function<bool(const std::filesystem::path&)>;

class FileTraverser {
public:
    void traverse(const std::filesystem::path& root, const FileCallback& onFile) const;
};

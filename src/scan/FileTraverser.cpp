#include "scan/FileTraverser.h"
#include <algorithm>
#include <vector>

static bool traverseImpl(const std::filesystem::path& root, const FileCallback& onFile) {
    std::error_code ec;
    auto status = std::filesystem::symlink_status(root, ec);
    if (ec) return true; // inaccessible — skip, continue

    if (status.type() == std::filesystem::file_type::regular)
        return onFile(root);

    if (status.type() != std::filesystem::file_type::directory)
        return true; // symlink or special file — skip

    std::filesystem::directory_iterator it(root, ec);
    if (ec) return true; // cannot open directory — skip

    std::vector<std::filesystem::path> entries;
    for (const auto& entry : it)
        entries.push_back(entry.path());

    std::sort(entries.begin(), entries.end());

    for (const auto& entry : entries) {
        if (!traverseImpl(entry, onFile))
            return false; // callback requested stop
    }
    return true;
}

void FileTraverser::traverse(const std::filesystem::path& root, const FileCallback& onFile) const {
    traverseImpl(root, onFile);
}

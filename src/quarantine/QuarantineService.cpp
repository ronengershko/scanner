#include "quarantine/QuarantineService.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <uuid/uuid.h>

namespace fs = std::filesystem;

static std::string generateId() {
    uuid_t u;
    uuid_generate_random(u);
    char s[37];
    uuid_unparse_lower(u, s);
    return s;
}

static void moveFile(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::rename(src, dst, ec);
    if (!ec) return;

    // Cross-filesystem: copy then delete
    fs::copy_file(src, dst);
    fs::remove(src);
}

QuarantineService::QuarantineService(QuarantineRepository& repo, Logger& logger,
                                     const fs::path& quarantineDir)
    : m_repo(repo), m_logger(logger), m_quarantineDir(quarantineDir) {}

QuarantineItem QuarantineService::quarantine(const fs::path& path,
                                              const FileMetadata& metadata,
                                              std::optional<int64_t> signatureId) {
    std::string id = generateId();
    fs::path dst = m_quarantineDir / (id + ".bin");

    moveFile(path, dst);

    QuarantineItem item{
        id,
        dst.string(),
        metadata.normalizedPath.string(),
        static_cast<int64_t>(metadata.deviceId),
        static_cast<int64_t>(metadata.inode),
        static_cast<int64_t>(metadata.size),
        metadata.modificationTime,
        signatureId.value_or(0),
        "quarantined"
    };

    m_repo.add(item);
    m_logger.info("Quarantined: " + metadata.normalizedPath.string() + " id=" + id);
    return item;
}

void QuarantineService::list() const {
    auto items = m_repo.loadAll();
    if (items.empty()) {
        std::cout << "No quarantined files.\n";
        return;
    }
    std::cout << "ID                                    Status        Original Path\n";
    for (const auto& item : items)
        std::cout << item.id << "  " << std::left
                  << std::setw(12) << item.status << "  "
                  << item.originalPath << "\n";
}

void QuarantineService::restore(const std::string& id) {
    auto item = m_repo.findById(id);
    if (!item)
        throw std::runtime_error("Quarantine item not found: " + id);
    if (item->status != "quarantined")
        throw std::runtime_error("Item is not quarantined (status=" + item->status + ")");

    fs::path src(item->quarantinePath);
    fs::path dst(item->originalPath);

    if (!fs::exists(src))
        throw std::runtime_error("Quarantine file missing: " + src.string());
    if (!fs::exists(dst.parent_path()))
        throw std::runtime_error("Original directory does not exist: " + dst.parent_path().string());
    if (fs::exists(dst))
        throw std::runtime_error("File already exists at original path: " + dst.string());

    moveFile(src, dst);
    m_repo.updateStatus(id, "restored");
    m_logger.info("Restored: " + dst.string() + " id=" + id);
}

void QuarantineService::remove(const std::string& id) {
    auto item = m_repo.findById(id);
    if (!item)
        throw std::runtime_error("Quarantine item not found: " + id);
    if (item->status != "quarantined")
        throw std::runtime_error("Item is not quarantined (status=" + item->status + ")");

    fs::path qpath(item->quarantinePath);
    if (fs::exists(qpath))
        fs::remove(qpath);

    m_repo.updateStatus(id, "deleted");
    m_logger.info("Deleted quarantine item: " + id);
}

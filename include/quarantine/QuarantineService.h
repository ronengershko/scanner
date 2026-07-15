#pragma once
#include "database/QuarantineRepository.h"
#include "logging/Logger.h"
#include "scan/FileMetadataProvider.h"
#include <filesystem>
#include <optional>
#include <vector>

class QuarantineService {
public:
    QuarantineService(QuarantineRepository& repo, Logger& logger,
                      const std::filesystem::path& quarantineDir);

    QuarantineItem quarantine(const std::filesystem::path& path,
                              const FileMetadata& metadata,
                              std::optional<int64_t> signatureId);

    void list() const;
    void restore(const std::string& id);
    void remove(const std::string& id);

private:
    QuarantineRepository&  m_repo;
    Logger&                m_logger;
    std::filesystem::path  m_quarantineDir;
};

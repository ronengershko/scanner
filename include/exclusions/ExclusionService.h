#pragma once
#include "database/ExclusionRepository.h"
#include "logging/Logger.h"
#include <filesystem>

class ExclusionService {
public:
    ExclusionService(ExclusionRepository& repo, Logger& logger);

    void list() const;
    void add(const std::filesystem::path& path);
    void remove(int64_t id);
    bool isExcluded(const std::filesystem::path& path) const;

private:
    ExclusionRepository&   m_repo;
    Logger&                m_logger;
};

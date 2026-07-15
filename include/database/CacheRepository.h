#pragma once
#include "database/Database.h"
#include <optional>
#include <string>

struct CacheRecord {
    std::string path;
    int64_t deviceId;
    int64_t inode;
    int64_t size;
    int64_t modificationTime;
    int64_t signatureVersion;
    std::string verdict;
};

class CacheRepository {
public:
    explicit CacheRepository(Database& db);

    std::optional<CacheRecord> lookup(const std::string& path) const;
    void upsert(const CacheRecord& record);

private:
    Database& m_db;
};

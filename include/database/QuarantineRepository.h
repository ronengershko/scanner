#pragma once
#include "database/Database.h"
#include <optional>
#include <string>
#include <vector>

struct QuarantineItem {
    std::string id;
    std::string quarantinePath;
    std::string originalPath;
    int64_t originalDeviceId;
    int64_t originalInode;
    int64_t originalSize;
    int64_t originalModificationTime;
    int64_t detectedSignatureId;
    std::string status;
};

class QuarantineRepository {
public:
    explicit QuarantineRepository(Database& db);

    void add(const QuarantineItem& item);
    std::vector<QuarantineItem> loadAll() const;
    std::optional<QuarantineItem> findById(const std::string& id) const;
    void updateStatus(const std::string& id, const std::string& status);
    void remove(const std::string& id);

private:
    Database& m_db;
};

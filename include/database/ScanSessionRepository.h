#pragma once
#include "database/Database.h"
#include <optional>
#include <string>

struct ScanSession {
    int64_t id;
    std::string requestedPath;
    std::string canonicalPath;
    std::string scanType;
    std::string status;
    std::string lastCompletedPath;
};

class ScanSessionRepository {
public:
    explicit ScanSessionRepository(Database& db);

    int64_t create(const std::string& requestedPath,
                   const std::string& canonicalPath,
                   const std::string& scanType);

    std::optional<ScanSession> findRunning() const;
    std::optional<ScanSession> findStopped() const;

    void updateStatus(int64_t id, const std::string& status);
    void updateCheckpoint(int64_t id, const std::string& lastCompletedPath);
    void updateCounters(int64_t id, int64_t scanned, int64_t cacheHits,
                        int64_t malicious, int64_t excluded, int64_t errors);
    std::string getStatus(int64_t id) const;

    std::string getScanRoot() const;
    void setScanRoot(const std::string& path);

private:
    Database& m_db;
    std::optional<ScanSession> findByStatus(const std::string& status) const;
};

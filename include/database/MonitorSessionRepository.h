#pragma once
#include "database/Database.h"
#include "database/RepositoryUtils.h"
#include <optional>
#include <string>
#include <vector>

struct WatchPath {
    int64_t id;
    std::string path;
};

struct MonitorSession {
    int64_t id;
    std::string status;
    int64_t pid;
    std::string startedAt;
    std::string stoppedAt;
};

class MonitorSessionRepository {
public:
    explicit MonitorSessionRepository(Database& db);

    int64_t create(int64_t pid);
    void updateStatus(int64_t id, const std::string& status);
    std::optional<MonitorSession> findRunning() const;
    void clearRunning();
    std::vector<MonitorSession> listRecent(int n) const;

    std::vector<WatchPath> loadAllWatchPaths() const;
    int64_t addWatchPath(const std::string& path);
    void removeWatchPath(int64_t id);

private:
    Database& m_db;
};

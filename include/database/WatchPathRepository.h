#pragma once
#include "database/Database.h"
#include "database/RepositoryUtils.h"
#include <string>
#include <vector>

struct WatchPath {
    int64_t id;
    std::string path;
};

class WatchPathRepository {
public:
    explicit WatchPathRepository(Database& db);
    std::vector<WatchPath> loadAll() const;
    int64_t add(const std::string& path);
    void remove(int64_t id);

private:
    Database& m_db;
};

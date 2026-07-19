#pragma once
#include "database/Database.h"
#include <string>
#include <vector>

struct Exclusion {
    int64_t id;
    std::string path;
    bool isDirectory;
};

class ExclusionRepository {
public:
    explicit ExclusionRepository(Database& db);

    std::vector<Exclusion> loadAll() const;
    int64_t add(const std::string& path, bool isDirectory);
    void remove(int64_t id);
    void seedIfMissing(const std::string& path, bool isDirectory);

private:
    Database& m_db;
};

#pragma once
#include "database/Database.h"
#include <string>
#include <vector>

struct SignatureRecord {
    int64_t id;
    std::string value;
};

class SignatureRepository {
public:
    explicit SignatureRepository(Database& db);

    std::vector<SignatureRecord> loadAll() const;
    int64_t add(const std::string& value);
    void remove(int64_t id);

    int64_t getVersion() const;
    void incrementVersion();

private:
    Database& m_db;
};

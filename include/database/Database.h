#pragma once
#include <filesystem>
#include <string>
#include <sqlite3.h>

// RAII wrapper for sqlite3_stmt
struct Stmt {
    sqlite3_stmt* ptr = nullptr;
    Stmt() = default;
    ~Stmt() { sqlite3_finalize(ptr); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
};

class Database {
public:
    explicit Database(const std::filesystem::path& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void initializeSchema();
    void beginTransaction();
    void commit();
    void rollback();

    sqlite3* handle() const;

private:
    void execute(const std::string& sql);
    sqlite3* m_db = nullptr;
};

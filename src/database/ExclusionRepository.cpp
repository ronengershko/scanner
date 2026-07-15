#include "database/ExclusionRepository.h"
#include <chrono>
#include <ctime>
#include <stdexcept>

static std::string nowIso() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

static void check(int rc, sqlite3* db) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(db));
}

ExclusionRepository::ExclusionRepository(Database& db) : m_db(db) {}

std::vector<Exclusion> ExclusionRepository::loadAll() const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, path, is_directory FROM exclusions",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::vector<Exclusion> result;
    while (sqlite3_step(stmt.ptr) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int64(stmt.ptr, 0),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 1)),
            sqlite3_column_int(stmt.ptr, 2) != 0
        });
    }
    return result;
}

int64_t ExclusionRepository::add(const std::string& path, bool isDirectory) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "INSERT INTO exclusions (path, is_directory, created_at) VALUES (?, ?, ?)",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_text(stmt.ptr, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt.ptr, 2, isDirectory ? 1 : 0);
    sqlite3_bind_text(stmt.ptr, 3, ts.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));

    return sqlite3_last_insert_rowid(m_db.handle());
}

void ExclusionRepository::remove(int64_t id) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "DELETE FROM exclusions WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_int64(stmt.ptr, 1, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

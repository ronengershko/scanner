#include "database/WatchPathRepository.h"
#include <stdexcept>

WatchPathRepository::WatchPathRepository(Database& db) : m_db(db) {}

std::vector<WatchPath> WatchPathRepository::loadAll() const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, path FROM watch_paths ORDER BY id",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::vector<WatchPath> result;
    while (sqlite3_step(stmt.ptr) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int64(stmt.ptr, 0),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 1))
        });
    }
    return result;
}

int64_t WatchPathRepository::add(const std::string& path) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "INSERT INTO watch_paths (path, created_at) VALUES (?, ?)",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_text(stmt.ptr, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 2, ts.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));

    return sqlite3_last_insert_rowid(m_db.handle());
}

void WatchPathRepository::remove(int64_t id) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "DELETE FROM watch_paths WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_int64(stmt.ptr, 1, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
    if (sqlite3_changes(m_db.handle()) == 0)
        throw std::runtime_error("Watch path not found: " + std::to_string(id));
}

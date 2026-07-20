#include "database/MonitorSessionRepository.h"
#include <stdexcept>

MonitorSessionRepository::MonitorSessionRepository(Database& db) : m_db(db) {}

int64_t MonitorSessionRepository::create(int64_t pid) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "INSERT INTO monitor_sessions (status, pid, started_at) VALUES ('running', ?, ?)",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_int64(stmt.ptr, 1, pid);
    sqlite3_bind_text(stmt.ptr, 2, ts.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
    return sqlite3_last_insert_rowid(m_db.handle());
}

void MonitorSessionRepository::updateStatus(int64_t id, const std::string& status) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "UPDATE monitor_sessions SET status = ?, stopped_at = ? WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_text(stmt.ptr, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 2, ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.ptr, 3, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

std::optional<MonitorSession> MonitorSessionRepository::findRunning() const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, status, pid, started_at, COALESCE(stopped_at, '') "
        "FROM monitor_sessions WHERE status = 'running' LIMIT 1",
        -1, &stmt.ptr, nullptr), m_db.handle());

    if (sqlite3_step(stmt.ptr) != SQLITE_ROW)
        return std::nullopt;

    return MonitorSession{
        sqlite3_column_int64(stmt.ptr, 0),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 1)),
        sqlite3_column_int64(stmt.ptr, 2),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 3)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 4))
    };
}

std::vector<MonitorSession> MonitorSessionRepository::listRecent(int n) const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, status, pid, started_at, COALESCE(stopped_at, '') "
        "FROM monitor_sessions ORDER BY id DESC LIMIT ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_int(stmt.ptr, 1, n);

    std::vector<MonitorSession> result;
    while (sqlite3_step(stmt.ptr) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int64(stmt.ptr, 0),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 1)),
            sqlite3_column_int64(stmt.ptr, 2),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 3)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 4))
        });
    }
    return result;
}

std::vector<WatchPath> MonitorSessionRepository::loadAllWatchPaths() const {
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

int64_t MonitorSessionRepository::addWatchPath(const std::string& path) {
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

void MonitorSessionRepository::removeWatchPath(int64_t id) {
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

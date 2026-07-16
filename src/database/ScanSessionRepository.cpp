#include "database/ScanSessionRepository.h"
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

ScanSessionRepository::ScanSessionRepository(Database& db) : m_db(db) {}

int64_t ScanSessionRepository::create(const std::string& requestedPath,
                                      const std::string& canonicalPath,
                                      const std::string& scanType) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "INSERT INTO scan_sessions (requested_path, canonical_path, scan_type, status, created_at)"
        " VALUES (?, ?, ?, 'running', ?)",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_text(stmt.ptr, 1, requestedPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 2, canonicalPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 3, scanType.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 4, ts.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));

    return sqlite3_last_insert_rowid(m_db.handle());
}

std::optional<ScanSession> ScanSessionRepository::findByStatus(const std::string& status) const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, requested_path, canonical_path, scan_type, status,"
        " COALESCE(last_completed_path, '')"
        " FROM scan_sessions WHERE status = ? ORDER BY id DESC LIMIT 1",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, status.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_ROW)
        return std::nullopt;

    return ScanSession{
        sqlite3_column_int64(stmt.ptr, 0),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 1)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 2)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 3)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 4)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 5))
    };
}

std::optional<ScanSession> ScanSessionRepository::findRunning() const {
    return findByStatus("running");
}

std::optional<ScanSession> ScanSessionRepository::findStopped() const {
    return findByStatus("stopped");
}

void ScanSessionRepository::updateStatus(int64_t id, const std::string& status) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "UPDATE scan_sessions SET status = ? WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.ptr, 2, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

void ScanSessionRepository::updateCheckpoint(int64_t id, const std::string& lastCompletedPath) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "UPDATE scan_sessions SET last_completed_path = ? WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, lastCompletedPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.ptr, 2, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

void ScanSessionRepository::updateCounters(int64_t id, int64_t scanned,
                                           int64_t cacheHits, int64_t malicious,
                                           int64_t excluded, int64_t errors) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "UPDATE scan_sessions"
        " SET scanned_files   = scanned_files   + ?,"
        "     cache_hits      = cache_hits      + ?,"
        "     malicious_files = malicious_files + ?,"
        "     excluded_files  = excluded_files  + ?,"
        "     error_files     = error_files     + ?"
        " WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_int64(stmt.ptr, 1, scanned);
    sqlite3_bind_int64(stmt.ptr, 2, cacheHits);
    sqlite3_bind_int64(stmt.ptr, 3, malicious);
    sqlite3_bind_int64(stmt.ptr, 4, excluded);
    sqlite3_bind_int64(stmt.ptr, 5, errors);
    sqlite3_bind_int64(stmt.ptr, 6, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

std::string ScanSessionRepository::getStatus(int64_t id) const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT status FROM scan_sessions WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_int64(stmt.ptr, 1, id);

    if (sqlite3_step(stmt.ptr) != SQLITE_ROW)
        throw std::runtime_error("Session not found: " + std::to_string(id));

    return reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 0));
}

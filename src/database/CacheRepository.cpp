#include "database/CacheRepository.h"
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

CacheRepository::CacheRepository(Database& db) : m_db(db) {}

std::optional<CacheRecord> CacheRepository::lookup(const std::string& path) const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT path, device_id, inode, size, modification_time, signature_version, verdict"
        " FROM file_cache WHERE path = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, path.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_ROW)
        return std::nullopt;

    return CacheRecord{
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 0)),
        sqlite3_column_int64(stmt.ptr, 1),
        sqlite3_column_int64(stmt.ptr, 2),
        sqlite3_column_int64(stmt.ptr, 3),
        sqlite3_column_int64(stmt.ptr, 4),
        sqlite3_column_int64(stmt.ptr, 5),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 6))
    };
}

void CacheRepository::upsert(const CacheRecord& record) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "INSERT OR REPLACE INTO file_cache"
        " (path, device_id, inode, size, modification_time, signature_version, verdict, scanned_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_text(stmt.ptr, 1, record.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.ptr, 2, record.deviceId);
    sqlite3_bind_int64(stmt.ptr, 3, record.inode);
    sqlite3_bind_int64(stmt.ptr, 4, record.size);
    sqlite3_bind_int64(stmt.ptr, 5, record.modificationTime);
    sqlite3_bind_int64(stmt.ptr, 6, record.signatureVersion);
    sqlite3_bind_text(stmt.ptr, 7, record.verdict.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 8, ts.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

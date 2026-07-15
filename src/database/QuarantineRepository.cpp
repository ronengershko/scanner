#include "database/QuarantineRepository.h"
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

static QuarantineItem rowToItem(sqlite3_stmt* stmt) {
    return {
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        sqlite3_column_int64(stmt, 3),
        sqlite3_column_int64(stmt, 4),
        sqlite3_column_int64(stmt, 5),
        sqlite3_column_int64(stmt, 6),
        sqlite3_column_int64(stmt, 7),
        reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8))
    };
}

QuarantineRepository::QuarantineRepository(Database& db) : m_db(db) {}

void QuarantineRepository::add(const QuarantineItem& item) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "INSERT INTO quarantine_items"
        " (id, quarantine_path, original_path, original_device_id, original_inode,"
        "  original_size, original_modification_time, detected_signature_id, quarantined_at, status)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::string ts = nowIso();
    sqlite3_bind_text(stmt.ptr, 1,  item.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 2,  item.quarantinePath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 3,  item.originalPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.ptr, 4, item.originalDeviceId);
    sqlite3_bind_int64(stmt.ptr, 5, item.originalInode);
    sqlite3_bind_int64(stmt.ptr, 6, item.originalSize);
    sqlite3_bind_int64(stmt.ptr, 7, item.originalModificationTime);
    sqlite3_bind_int64(stmt.ptr, 8, item.detectedSignatureId);
    sqlite3_bind_text(stmt.ptr, 9,  ts.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 10, item.status.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

std::vector<QuarantineItem> QuarantineRepository::loadAll() const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, quarantine_path, original_path, original_device_id, original_inode,"
        " original_size, original_modification_time, COALESCE(detected_signature_id, 0), status"
        " FROM quarantine_items",
        -1, &stmt.ptr, nullptr), m_db.handle());

    std::vector<QuarantineItem> result;
    while (sqlite3_step(stmt.ptr) == SQLITE_ROW)
        result.push_back(rowToItem(stmt.ptr));
    return result;
}

std::optional<QuarantineItem> QuarantineRepository::findById(const std::string& id) const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, quarantine_path, original_path, original_device_id, original_inode,"
        " original_size, original_modification_time, COALESCE(detected_signature_id, 0), status"
        " FROM quarantine_items WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_ROW)
        return std::nullopt;

    return rowToItem(stmt.ptr);
}

void QuarantineRepository::remove(const std::string& id) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "DELETE FROM quarantine_items WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

void QuarantineRepository::updateStatus(const std::string& id, const std::string& status) {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "UPDATE quarantine_items SET status = ? WHERE id = ?",
        -1, &stmt.ptr, nullptr), m_db.handle());

    sqlite3_bind_text(stmt.ptr, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.ptr, 2, id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

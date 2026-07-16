#include "database/SignatureRepository.h"
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

SignatureRepository::SignatureRepository(Database& db) : m_db(db) {}

std::vector<SignatureRecord> SignatureRepository::loadAll() const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT id, value FROM signatures", -1, &stmt.ptr, nullptr),
        m_db.handle());

    std::vector<SignatureRecord> result;
    while (sqlite3_step(stmt.ptr) == SQLITE_ROW) {
        result.push_back({
            sqlite3_column_int64(stmt.ptr, 0),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 1))
        });
    }
    return result;
}

int64_t SignatureRepository::add(const std::string& value) {
    m_db.beginTransaction();
    try {
        Stmt stmt;
        check(sqlite3_prepare_v2(m_db.handle(),
            "INSERT INTO signatures (value, created_at) VALUES (?, ?)",
            -1, &stmt.ptr, nullptr), m_db.handle());

        std::string ts = nowIso();
        sqlite3_bind_text(stmt.ptr, 1, value.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.ptr, 2, ts.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt.ptr);
        if (rc == SQLITE_CONSTRAINT)
            throw std::runtime_error("Signature already exists: " + value);
        if (rc != SQLITE_DONE)
            throw std::runtime_error(sqlite3_errmsg(m_db.handle()));

        int64_t id = sqlite3_last_insert_rowid(m_db.handle());
        incrementVersion();
        m_db.commit();
        return id;
    } catch (...) {
        m_db.rollback();
        throw;
    }
}

void SignatureRepository::remove(int64_t id) {
    m_db.beginTransaction();
    try {
        Stmt stmt;
        check(sqlite3_prepare_v2(m_db.handle(),
            "DELETE FROM signatures WHERE id = ?",
            -1, &stmt.ptr, nullptr), m_db.handle());

        sqlite3_bind_int64(stmt.ptr, 1, id);

        if (sqlite3_step(stmt.ptr) != SQLITE_DONE)
            throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
        if (sqlite3_changes(m_db.handle()) == 0)
            throw std::runtime_error("Signature not found: " + std::to_string(id));

        incrementVersion();
        m_db.commit();
    } catch (...) {
        m_db.rollback();
        throw;
    }
}

int64_t SignatureRepository::getVersion() const {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "SELECT value FROM scanner_metadata WHERE key = 'signature_version'",
        -1, &stmt.ptr, nullptr), m_db.handle());

    if (sqlite3_step(stmt.ptr) != SQLITE_ROW)
        throw std::runtime_error("signature_version not found in metadata");

    return std::stoll(reinterpret_cast<const char*>(sqlite3_column_text(stmt.ptr, 0)));
}

void SignatureRepository::incrementVersion() {
    Stmt stmt;
    check(sqlite3_prepare_v2(m_db.handle(),
        "UPDATE scanner_metadata SET value = CAST(CAST(value AS INTEGER) + 1 AS TEXT) WHERE key = 'signature_version'",
        -1, &stmt.ptr, nullptr), m_db.handle());

    int rc = sqlite3_step(stmt.ptr);
    if (rc != SQLITE_DONE)
        throw std::runtime_error(sqlite3_errmsg(m_db.handle()));
}

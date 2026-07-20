#include "database/Database.h"
#include <stdexcept>

Database::Database(const std::filesystem::path& path) {
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::string msg = sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        throw std::runtime_error("Cannot open database: " + msg);
    }
    sqlite3_busy_timeout(m_db, 5000);
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");
}

Database::~Database() {
    sqlite3_close(m_db);
}

sqlite3* Database::handle() const { return m_db; }

void Database::execute(const std::string& sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err;
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

void Database::beginTransaction() { execute("BEGIN"); }
void Database::commit()           { execute("COMMIT"); }
void Database::rollback()         { execute("ROLLBACK"); }

void Database::initializeSchema() {
    execute(R"(
        CREATE TABLE IF NOT EXISTS signatures (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            value TEXT NOT NULL UNIQUE,
            created_at TEXT NOT NULL
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS scanner_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )
    )");

    execute("INSERT OR IGNORE INTO scanner_metadata (key, value) VALUES ('signature_version', '1')");
    execute("INSERT OR IGNORE INTO scanner_metadata (key, value) VALUES ('scan_root', '')");

    execute(R"(
        CREATE TABLE IF NOT EXISTS file_cache (
            path TEXT PRIMARY KEY,
            device_id INTEGER NOT NULL,
            inode INTEGER NOT NULL,
            size INTEGER NOT NULL,
            modification_time INTEGER NOT NULL,
            signature_version INTEGER NOT NULL,
            verdict TEXT NOT NULL,
            scanned_at TEXT NOT NULL
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS scan_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            requested_path TEXT NOT NULL,
            canonical_path TEXT NOT NULL,
            scan_type TEXT NOT NULL,
            status TEXT NOT NULL,
            last_completed_path TEXT,
            created_at TEXT NOT NULL,
            started_at TEXT,
            stopped_at TEXT,
            completed_at TEXT,
            scanned_files INTEGER NOT NULL DEFAULT 0,
            cache_hits INTEGER NOT NULL DEFAULT 0,
            malicious_files INTEGER NOT NULL DEFAULT 0,
            excluded_files INTEGER NOT NULL DEFAULT 0,
            skipped_files INTEGER NOT NULL DEFAULT 0,
            error_files INTEGER NOT NULL DEFAULT 0
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS exclusions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT NOT NULL UNIQUE,
            is_directory INTEGER NOT NULL,
            created_at TEXT NOT NULL
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS watch_paths (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT NOT NULL UNIQUE,
            created_at TEXT NOT NULL
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS monitor_sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            status TEXT NOT NULL,
            pid INTEGER NOT NULL,
            started_at TEXT NOT NULL,
            stopped_at TEXT
        )
    )");

    execute(R"(
        CREATE TABLE IF NOT EXISTS quarantine_items (
            id TEXT PRIMARY KEY,
            quarantine_path TEXT NOT NULL UNIQUE,
            original_path TEXT NOT NULL,
            original_device_id INTEGER,
            original_inode INTEGER,
            original_size INTEGER,
            original_modification_time INTEGER,
            detected_signature_id INTEGER,
            quarantined_at TEXT NOT NULL,
            status TEXT NOT NULL
        )
    )");
}

# Step 4 — SQLite Database and Repositories

## What Was Done

Added SQLite persistence. `Database` owns the connection via RAII. Five repositories handle all SQL — no SQL anywhere else.

### Files Created / Modified

```
include/database/Database.h              (new)
src/database/Database.cpp               (new)
include/database/SignatureRepository.h  (new)
src/database/SignatureRepository.cpp    (new)
include/database/ScanSessionRepository.h (new)
src/database/ScanSessionRepository.cpp  (new)
include/database/CacheRepository.h      (new)
src/database/CacheRepository.cpp        (new)
include/database/ExclusionRepository.h  (new)
src/database/ExclusionRepository.cpp    (new)
include/database/QuarantineRepository.h (new)
src/database/QuarantineRepository.cpp   (new)
include/app/Application.h               (updated — takes Database&)
src/app/Application.cpp                 (updated)
src/main.cpp                            (updated — constructs DB, calls initializeSchema)
CMakeLists.txt                          (updated — links sqlite3)
```

### Design

- `Database` destructor calls `sqlite3_close` — connection always released.
- `Stmt` (defined in `Database.h`) is a small RAII struct that calls `sqlite3_finalize` — no statement leaks on exceptions.
- All repositories use `sqlite3_prepare_v2` with bound parameters — no string concatenation, no SQL injection.
- `initializeSchema()` uses `CREATE TABLE IF NOT EXISTS` throughout — safe to call on every startup.
- `INSERT OR IGNORE INTO scanner_metadata` seeds `signature_version = 1` only once.
- `Database` is passed to `Application` by reference so all future services can share it.

### Tables created

```
signatures        file_cache       scan_sessions
scanner_metadata  exclusions       quarantine_items
```

---

## How to Build

```bash
./build.sh
```

---

## How to Test

### DB file and all 6 tables created on first run

```bash
rm -f scanner-data/scanner.db
./build/scanner stop
sqlite3 scanner-data/scanner.db ".tables"
# Expected: all 6 table names
```

### signature_version seeded to 1

```bash
sqlite3 scanner-data/scanner.db "SELECT key, value FROM scanner_metadata;"
# Expected: signature_version|1
```

### Schema is idempotent

```bash
./build/scanner stop   # second run
echo $?                # expect 0
```

### Signature CRUD + version increment

```bash
sqlite3 scanner-data/scanner.db "INSERT INTO signatures (value, created_at) VALUES ('evil_payload', datetime('now'));"
sqlite3 scanner-data/scanner.db "SELECT id, value FROM signatures;"
sqlite3 scanner-data/scanner.db "UPDATE scanner_metadata SET value = CAST(CAST(value AS INTEGER) + 1 AS TEXT) WHERE key = 'signature_version';"
sqlite3 scanner-data/scanner.db "SELECT value FROM scanner_metadata WHERE key = 'signature_version';"
# Expected: 2
```

### Scan session create / checkpoint / status update

```bash
sqlite3 scanner-data/scanner.db "INSERT INTO scan_sessions (requested_path, canonical_path, scan_type, status, created_at) VALUES ('/tmp', '/tmp', 'path', 'running', datetime('now'));"
sqlite3 scanner-data/scanner.db "UPDATE scan_sessions SET last_completed_path = '/tmp/a.txt', status = 'stopped' WHERE id = 1;"
sqlite3 scanner-data/scanner.db "SELECT id, status, last_completed_path FROM scan_sessions;"
```

### Data persists across restarts

```bash
./build/scanner stop
sqlite3 scanner-data/scanner.db "SELECT COUNT(*) FROM scan_sessions;"
# Expected: same count as before restart
```

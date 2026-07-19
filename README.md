# Sentinel Scanner

A command-line malware scanner for macOS, written in C++17 with SQLite for persistence.

## Build

```bash
cmake -B build -S .
cmake --build build
./build/scanner <command>
```

## CLI Style program


```
scanner scan --path <path>       scan a file or directory
scanner scan --all               scan the configured root (set with config set-root)
scanner stop                     signal a running scan to stop
scanner resume                   continue from the last stopped scan's checkpoint

scanner signatures list
scanner signatures add <value>
scanner signatures remove <id>

scanner exclusions list
scanner exclusions add <path>
scanner exclusions remove <id>

scanner quarantine list
scanner quarantine restore <id>
scanner quarantine delete <id>

scanner config set-root <path>   persist the full-scan root in the DB
```

## Architecture

Strict layered design: CLI → Services → Repositories → Platform. 

```
main.cpp                         constructs all objects, calls app.run()
  └── Application                parses CLI, dispatches to services
        ├── ScanService          traversal, caching, quarantine decisions
        ├── SignatureService      load/add/remove signatures
        ├── ExclusionService      isExcluded checks, add/remove exclusions
        └── QuarantineService     move files to quarantine, restore, delete

Repositories (database/)         all SQL lives here
  ├── SignatureRepository
  ├── ScanSessionRepository       also owns scan_root config
  ├── CacheRepository
  ├── ExclusionRepository
  └── QuarantineRepository

Platform layer
  ├── FileTraverser               recursive alphabetical walk, skips symlinks
  ├── FileScanner                 chunk-based binary search across signatures
  └── FileMetadataProvider        wraps stat() — returns inode, device, size, mtime
```

## What happens when a file is scanned

For every file the traverser visits, `ScanService::processFile` runs these steps in order:

1. **Exclusion check** — ask `ExclusionService::isExcluded(file)`. If the file's canonical path matches an exclusion (exact for files, prefix-component for directories), count it as excluded and skip.
2. **Metadata** — call `stat()` via `FileMetadataProvider` to get device ID, inode, size, and modification time.
3. **Cache lookup** — query `file_cache` by path. A cache hit requires all five fields to match: `(device_id, inode, size, mtime, signature_version)`. If hit, reuse the stored verdict without re-scanning.
4. **Scan** — `FileScanner::scan()` reads the file in 65536-byte chunks. At each boundary it keeps a `maxSigLen-1` byte overlap so signatures are never missed across chunk boundaries. Uses `std::search` against all signatures per window. Stops on first match.
5. **Stability check** — re-read metadata after the scan. If the file changed while being read, retry once with fresh metadata.
6. **Cache write** — store the verdict keyed by path.
7. **Quarantine** — if malicious, `QuarantineService` moves the file to `scanner-data/quarantine/` and records it in the DB.
8. **Checkpoint** — write the file's path as `last_completed_path` in the session row.

## Runtime complexity

- **Scan one file**: O(file\_size × s) — file read once, s signatures searched per chunk.
- **Cache hit**: O(1) — single indexed DB lookup by path. After the first full scan, most files hit the cache unless they change or a signature is added.
- **Exclusion check**: O(k) per file — k exclusion rules loaded from DB each call.
- **Full scan**: O(n × file\_size × s) worst case; O(n) amortized when cache is warm.
- **Traversal**: O(n log n) — one `std::sort` per directory level.

Adding or removing a signature increments the `signature_version`. This invalidates all cache entries — every file must be re-scanned on the next run.

## Stop and resume

`scanner stop` sets the running session's status to `stop_requested` in the DB. The scanning process polls `getStatus()` before each file and stops when it sees that flag, writing the last completed file path as the checkpoint.

`scanner resume` finds the most recent stopped session and re-runs the same path from the checkpoint forward. It skips files whose canonical path is ≤ the checkpoint, using `std::filesystem::path` component-wise comparison — which matches alphabetical traversal order exactly.

## Data files

```
scanner-data/
├── scanner.db         SQLite — signatures, sessions, cache, quarantine, exclusions
├── scanner.log        append-only log (INFO / WARNING / ERROR)
└── quarantine/        quarantined files are moved here (not copied)
```

`scanner-data/` is auto-excluded at startup so the scanner never scans its own DB or log.

## File identity

Files are tracked by `(device_id, inode)`, not by path. Renaming a file does not invalidate the cache entry as long as the content and metadata are unchanged.

## macOS assumptions

This project targets macOS and makes several platform-specific choices:

| Concern | macOS (this project) | Linux | Windows |
|---|---|---|---|
| File identity | `st_dev + st_ino` from `stat()` | Same | File index from `GetFileInformationByHandle()`, or path-only |
| Auto-start at boot | `launchd` LaunchDaemon plist | `systemd` unit file | Windows Service or Task Scheduler |
| Auto-start at login | `launchd` LaunchAgent plist | `systemd --user` or `.bashrc` | Task Scheduler (on logon trigger) |
| Protected dirs | TCC / Full Disk Access | `chmod` / capability | ACLs |

## Why there is no default full-scan root

Running `scan --all` without a configured root would scan an uncontrolled path and could quarantine a system file, making the OS or applications unusable. The root must be set explicitly with `config set-root`. Missing root is an error — the scan refuses to run.

## Auto-start setup

```bash
# Set what to scan
./build/scanner config set-root /path/to/scan

# LaunchAgent — runs at user login
cp launchd/com.sentinel.scanner.agent.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.sentinel.scanner.agent.plist

# LaunchDaemon — runs at OS boot, as root (requires sudo)
sudo cp launchd/com.sentinel.scanner.daemon.plist /Library/LaunchDaemons/
sudo launchctl load /Library/LaunchDaemons/com.sentinel.scanner.daemon.plist
```

## Tests

```bash
./run_tests.sh
```

59 tests across 4 suites using Google Test. Cleans `scanner-data/` before and after. All tests use isolated temp directories and never touch real user files or the production DB.

| Suite | Tests | Covers |
|---|---|---|
| `test_cli` | 18 | All commands parse correctly; invalid input throws |
| `test_scanner` | 14 | FileScanner clean/malicious/chunk-boundary/error, FileTraverser sort/stop/symlinks/permissions, FileMetadataProvider |
| `test_db` | 20 | Cache hit/miss logic, CacheRepository persistence, Signature add/remove/version/duplicate, Exclusion add/remove |
| `test_integration` | 7 | Clean scan, malicious+quarantine, directory count, cache hit on 2nd scan, exclusion counter, stop guard, resume from checkpoint |

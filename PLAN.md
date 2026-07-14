# Scanner — SentinelOne Home Assignment Implementation Plan

## 1. Assignment Demands

The program must meet these seven demands:

1. **Cache previous scan results**  
   Unchanged files must not be scanned again unnecessarily.

2. **Scan a specific path or the full scan root**  
   The user must be able to scan one file, one directory, or the configured full scan root.

3. **Quarantine malicious files**  
   Malicious files must be moved to quarantine. The user must be able to list, restore, and delete quarantined files.

4. **Change malicious strings without recompilation**  
   Detection strings must be stored outside the compiled code and managed through commands.

5. **Log important events**  
   The program must log meaningful activity and errors.

6. **Stop and resume a scan**  
   A scan must stop only after finishing the current file and later resume after the last completed file.

7. **Exclude files and directories**  
   The user must be able to configure files and directory trees that should not be scanned.

The scanner must also:

- run manually from the terminal;
- run automatically when the macOS user session starts;
- scan regular files only;
- ignore symbolic links and special files;
- continue after permission errors;
- allow only one active scan at a time.

---

# 2. Final Program Structure

```text
scanner/
├── CMakeLists.txt
├── README.md
├── PLAN.md
├── include/
│   ├── app/
│   │   ├── Application.h
│   │   └── AppConfig.h
│   ├── cli/
│   │   ├── Command.h
│   │   └── CommandLineParser.h
│   ├── database/
│   │   ├── Database.h
│   │   ├── ScanSessionRepository.h
│   │   ├── SignatureRepository.h
│   │   ├── CacheRepository.h
│   │   ├── ExclusionRepository.h
│   │   └── QuarantineRepository.h
│   ├── logging/
│   │   └── Logger.h
│   ├── scan/
│   │   ├── ScanService.h
│   │   ├── FileScanner.h
│   │   ├── FileTraverser.h
│   │   ├── FileMetadataProvider.h
│   │   └── ScanTypes.h
│   ├── signatures/
│   │   └── SignatureService.h
│   ├── exclusions/
│   │   └── ExclusionService.h
│   └── quarantine/
│       └── QuarantineService.h
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.cpp
│   │   └── AppConfig.cpp
│   ├── cli/
│   │   └── CommandLineParser.cpp
│   ├── database/
│   │   ├── Database.cpp
│   │   ├── ScanSessionRepository.cpp
│   │   ├── SignatureRepository.cpp
│   │   ├── CacheRepository.cpp
│   │   ├── ExclusionRepository.cpp
│   │   └── QuarantineRepository.cpp
│   ├── logging/
│   │   └── Logger.cpp
│   ├── scan/
│   │   ├── ScanService.cpp
│   │   ├── FileScanner.cpp
│   │   ├── FileTraverser.cpp
│   │   └── FileMetadataProvider.cpp
│   ├── signatures/
│   │   └── SignatureService.cpp
│   ├── exclusions/
│   │   └── ExclusionService.cpp
│   └── quarantine/
│       └── QuarantineService.cpp
├── tests/
│   ├── cli/
│   ├── scan/
│   ├── database/
│   ├── signatures/
│   ├── exclusions/
│   ├── quarantine/
│   └── integration/
└── launchd/
    └── com.sentinel.scanner.plist
```

---

# 3. Class Responsibilities

## `main.cpp`

Purpose:

- create the application configuration;
- create the database, logger, repositories, and services;
- create `Application`;
- pass `argc` and `argv` to `Application::run`;
- return the final exit code.

`main.cpp` must not contain scan logic, SQL, traversal logic, or command-specific business logic.

Example flow:

```cpp
int main(int argc, char* argv[]) {
    AppConfig config = AppConfig::load();
    Application app(config);
    return app.run(argc, argv);
}
```

---

## `Application`

Purpose:

- own or receive all top-level services;
- parse the command;
- dispatch the command to the correct service;
- convert thrown failures into user-facing errors and exit codes.

Example:

```text
scanner scan --path /tmp/test
        ↓
Application
        ↓
CommandLineParser
        ↓
ScanService::scanPath("/tmp/test")
```

`Application` is the program coordinator.

---

## `CommandLineParser`

Purpose:

- translate raw `argc` and `argv` values into a typed `Command`;
- validate command syntax;
- reject missing arguments and unknown commands.

It does not execute commands.

Example input:

```bash
scanner signatures add "evil_payload"
```

Example output:

```cpp
Command {
    type = CommandType::SignatureAdd,
    argument = "evil_payload"
}
```

---

## `Database`

Purpose:

- open and close the SQLite connection;
- initialize the database schema;
- execute prepared statements;
- provide transaction support;
- manage SQLite resources with RAII.

It does not contain scanner business rules.

---

## Repositories

Repositories perform database operations for one data type.

### `ScanSessionRepository`

Handles:

- creating scan sessions;
- loading running or stopped sessions;
- updating session status;
- storing checkpoint paths;
- updating counters.

### `SignatureRepository`

Handles:

- loading signatures;
- adding signatures;
- removing signatures;
- reading and incrementing the signature version.

### `CacheRepository`

Handles:

- looking up cached file verdicts;
- inserting or updating cache records;
- invalidating or removing unusable records.

### `ExclusionRepository`

Handles:

- storing exclusions;
- loading exclusions;
- removing exclusions.

### `QuarantineRepository`

Handles:

- storing quarantine metadata;
- loading quarantine items;
- updating quarantine status.

Repositories do not print output and do not make business decisions.

---

## `Logger`

Purpose:

- write timestamped log entries;
- support `INFO`, `WARNING`, and `ERROR`;
- append to the configured log file;
- centralize all logging behavior.

---

## `FileScanner`

Purpose:

- scan one regular file;
- read the file in binary chunks;
- search for configured byte signatures;
- detect signatures across chunk boundaries;
- return `Clean`, `Malicious`, or `Error`.

It does not traverse directories, write to SQLite, or move files.

---

## `FileTraverser`

Purpose:

- accept one file or directory;
- recursively visit regular files;
- skip symlinks and special files;
- sort directory entries deterministically;
- stop recursion into excluded directories;
- invoke a callback for each accepted file.

It does not scan file contents.

---

## `FileMetadataProvider`

Purpose:

- read normalized path;
- read device ID;
- read inode;
- read file size;
- read modification time;
- compare metadata before and after scanning.

---

## `ScanService`

Purpose:

- control the complete scan workflow;
- create scan sessions;
- traverse files;
- check exclusions;
- check cache;
- call `FileScanner`;
- quarantine malicious files;
- update cache;
- update scan progress;
- stop and resume safely;
- print scan summaries.

`ScanService` is the main business-logic class.

---

## `SignatureService`

Purpose:

- list signatures;
- add signatures;
- remove signatures;
- validate signature values;
- increment the signature version after every change;
- log signature changes.

---

## `ExclusionService`

Purpose:

- list exclusions;
- add exclusions;
- remove exclusions;
- normalize exclusion paths;
- decide whether a path is excluded.

---

## `QuarantineService`

Purpose:

- move malicious files into quarantine;
- generate quarantine IDs;
- store original metadata;
- list quarantine items;
- restore files;
- delete quarantined files;
- prevent unsafe overwrite during restore.

---

# 4. Implementation Steps

## Step 1 — Create the Project, CLI Types, Parser, and Application

### Purpose

Build the program entry structure exactly as it will remain in the final implementation.

### Files and Classes

```text
src/main.cpp
include/app/Application.h
src/app/Application.cpp
include/cli/Command.h
include/cli/CommandLineParser.h
src/cli/CommandLineParser.cpp
```

### `CommandType`

Must contain:

```cpp
enum class CommandType {
    ScanPath,
    ScanAll,
    Stop,
    Resume,
    SignatureList,
    SignatureAdd,
    SignatureRemove,
    ExclusionList,
    ExclusionAdd,
    ExclusionRemove,
    QuarantineList,
    QuarantineRestore,
    QuarantineDelete
};
```

### `Command`

Must contain:

```cpp
struct Command {
    CommandType type;
    std::optional<std::string> argument;
};
```

### Commands

```bash
scanner scan --path <path>
scanner scan --all
scanner stop
scanner resume
scanner signatures list
scanner signatures add <value>
scanner signatures remove <id>
scanner exclusions list
scanner exclusions add <path>
scanner exclusions remove <id>
scanner quarantine list
scanner quarantine restore <id>
scanner quarantine delete <id>
```

### Required Behavior

`CommandLineParser` must:

- parse all commands;
- reject unknown commands;
- reject missing arguments;
- reject extra unsupported arguments;
- return a typed `Command`;
- throw or return a clear parsing error.

`Application` must:

- call the parser;
- dispatch each command to a temporary handler;
- print a clear placeholder result;
- return `0` on valid placeholder execution;
- return non-zero on invalid syntax.

### Example

Input:

```bash
scanner scan --path "/Users/ronen/Test Files"
```

Parsed command:

```cpp
Command {
    type = CommandType::ScanPath,
    argument = "/Users/ronen/Test Files"
}
```

Temporary output:

```text
Scan path command received: /Users/ronen/Test Files
```

### Requirements for This Step

- The project compiles with CMake.
- `main.cpp` contains only construction and `Application::run`.
- CLI parsing is fully separated from execution.
- Every required command is represented by `CommandType`.
- No scan logic exists in the parser.
- No SQL exists in `Application`.
- All invalid command formats return a clear message.
- Paths with spaces work when quoted by the shell.

### Completion Criteria

- Every command can be parsed successfully.
- Invalid commands fail with non-zero exit code.
- Parser tests cover every command.
- `Application` dispatches every `CommandType`.
- `main.cpp` remains minimal.

---

## Step 2 — Implement Local Configuration and Data Paths

### Purpose

simply the configurations.
the file system path for scan --all
path to log file
path to quarantyDirectory
path to database


### Files and Classes

```text
include/app/AppConfig.h
src/app/AppConfig.cpp
```

### `AppConfig`

Must contain:

```cpp
struct AppConfig {
    std::filesystem::path databasePath;
    std::filesystem::path logPath;
    std::filesystem::path quarantineDirectory;
    std::filesystem::path fullScanRoot;

    static AppConfig load();
    void createRequiredDirectories() const;
};
```

### Runtime Layout

All program-owned data is stored inside the project:

```text
scanner/
├── scanner-data/
│   ├── scanner.db
│   ├── scanner.log
│   └── quarantine/
└── build/
    └── scanner
```

The executable is built as:

```text
build/scanner
```

The program data is stored as:

```text
scanner-data/scanner.db
scanner-data/scanner.log
scanner-data/quarantine/
```

A macOS application-data directory can be added later during the startup and deployment step. It is not part of the current implementation.

### Requirements for This Step

- All paths come from `AppConfig`.
- Required directories are created automatically.
- Errors creating directories are reported clearly.
- Paths are normalized before use.
- The full-scan root is a safe test directory.
- Database, log, and quarantine paths are marked as mandatory internal exclusions.
- No class hardcodes its own storage path.

### Example

```cpp
AppConfig config = AppConfig::load();
config.createRequiredDirectories();
```

Example configured paths:

```cpp
databasePath = "scanner-data/scanner.db";
logPath = "scanner-data/scanner.log";
quarantineDirectory = "scanner-data/quarantine";
fullScanRoot = "test-data";
```

### Completion Criteria

- Running the program creates `scanner-data/`.
- `scanner.db`, `scanner.log`, and `quarantine/` use the configured local paths.
- All configured paths are valid.
- No duplicate path configuration exists.
- Tests verify directory creation and failure handling.

---

## Step 3 — Implement Central Logging

### Purpose

Create the logging infrastructure before scan operations begin.

### Files and Classes

```text
include/logging/Logger.h
src/logging/Logger.cpp
```

### `Logger`

Must expose:

```cpp
enum class LogLevel {
    Info,
    Warning,
    Error
};

class Logger {
public:
    explicit Logger(const std::filesystem::path& logPath);

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
};
```

### Log Format

```text
2026-07-13 16:30:22 INFO Scan started path=/tmp/test
```

### Requirements for This Step

- Logs are appended.
- Every line contains timestamp, level, and message.
- Logging is thread-safe enough for the current single-scan design.
- Failure to open the log is surfaced clearly.
- Log messages never include file contents.
- `Application` logs startup and invalid commands.
- All future services receive the same logger instance.

### Completion Criteria

- Multiple runs append to the same log.
- `INFO`, `WARNING`, and `ERROR` are distinguishable.
- Logger tests verify format and append behavior.
- Application startup is logged.

---

## Step 4 — Implement SQLite and Repositories

### Purpose

Create persistent storage and repository boundaries before business logic.

### Files and Classes

```text
include/database/Database.h
src/database/Database.cpp

include/database/ScanSessionRepository.h
src/database/ScanSessionRepository.cpp

include/database/SignatureRepository.h
src/database/SignatureRepository.cpp

include/database/CacheRepository.h
src/database/CacheRepository.cpp

include/database/ExclusionRepository.h
src/database/ExclusionRepository.cpp

include/database/QuarantineRepository.h
src/database/QuarantineRepository.cpp
```

### Required Tables

#### `signatures`

```sql
CREATE TABLE signatures (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    value TEXT NOT NULL UNIQUE,
    created_at TEXT NOT NULL
);
```

#### `scanner_metadata`

```sql
CREATE TABLE scanner_metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
```

Initial row:

```text
signature_version = 1
```

#### `file_cache`

```sql
CREATE TABLE file_cache (
    path TEXT PRIMARY KEY,
    device_id INTEGER NOT NULL,
    inode INTEGER NOT NULL,
    size INTEGER NOT NULL,
    modification_time INTEGER NOT NULL,
    signature_version INTEGER NOT NULL,
    verdict TEXT NOT NULL,
    scanned_at TEXT NOT NULL
);
```

#### `scan_sessions`

```sql
CREATE TABLE scan_sessions (
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
);
```

#### `exclusions`

```sql
CREATE TABLE exclusions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT NOT NULL UNIQUE,
    is_directory INTEGER NOT NULL,
    created_at TEXT NOT NULL
);
```

#### `quarantine_items`

```sql
CREATE TABLE quarantine_items (
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
);
```

### Requirements for This Step

- `Database` owns the SQLite connection through RAII.
- Schema creation is idempotent.
- Prepared statements are used.
- Repositories contain SQL.
- Services do not contain SQL.
- Transactions are supported.
- SQL failures are logged and propagated.
- Data survives application restart.
- No pending-files table is created.

### Example

```cpp
Database database(config.databasePath);
database.initializeSchema();

SignatureRepository signatures(database);
ScanSessionRepository sessions(database);
```

### Completion Criteria

- Fresh database initializes successfully.
- Restart does not delete existing data.
- Every repository supports basic create/read/update operations.
- Transaction rollback is tested.
- Database resources close automatically.

---

## Step 5 — Implement Signature Storage and One-File Scanning

### Purpose

Make one-file malicious-string detection work correctly.

### Files and Classes

```text
include/scan/ScanTypes.h
include/scan/FileScanner.h
src/scan/FileScanner.cpp
include/signatures/SignatureService.h
src/signatures/SignatureService.cpp
```

### Types

```cpp
enum class Verdict {
    Clean,
    Malicious,
    Error
};

struct Signature {
    int64_t id;
    std::vector<std::byte> value;
};

struct ScanResult {
    Verdict verdict;
    std::optional<int64_t> matchedSignatureId;
    std::string errorMessage;
};
```

### `FileScanner`

Must expose:

```cpp
class FileScanner {
public:
    ScanResult scan(
        const std::filesystem::path& file,
        const std::vector<Signature>& signatures
    ) const;
};
```

### Required Scan Algorithm

1. Open the file in binary mode.
2. Read fixed-size chunks.
3. Search every configured signature.
4. Preserve `longest_signature_length - 1` bytes between chunks.
5. Return immediately when a signature matches.
6. Return clean only after successful end-of-file.
7. Return error for incomplete or failed reads.

### Example

Signatures:

```text
evil_payload
dangerous_text
```

File contents:

```text
normal data evil_payload more data
```

Result:

```cpp
ScanResult {
    verdict = Verdict::Malicious,
    matchedSignatureId = 1
}
```

### Requirements for This Step

- Signatures are loaded from SQLite.
- Empty signatures are rejected.
- Duplicate signatures are rejected.
- Files are read in binary mode.
- Large files are not loaded fully into memory.
- Binary zero bytes are supported.
- Chunk-boundary matches are detected.
- The scanner never modifies the file.
- The scanner returns the matching signature ID.
- No configured signatures means every readable file is clean.

### Completion Criteria

Tests cover:

- clean file;
- malicious file;
- binary file;
- empty file;
- unreadable file;
- match at beginning;
- match at end;
- match across chunk boundary;
- multiple signatures;
- no signatures.

---

## Step 6 — Implement Deterministic Filesystem Traversal

### Purpose

Visit scan candidates safely and in stable order. alpahbetic order.

### Files and Classes

```text
include/scan/FileTraverser.h
src/scan/FileTraverser.cpp
```

### `FileTraverser`

Must expose a callback-based interface:

```cpp
using FileCallback =
    std::function<bool(const std::filesystem::path&)>;

class FileTraverser {
public:
    void traverse(
        const std::filesystem::path& root,
        const FileCallback& onFile
    ) const;
};
```

Callback return value:

- `true`: continue traversal;
- `false`: stop traversal.

### Traversal Rules

- If root is one regular file, visit it once.
- If root is a directory, recurse.
- Sort entries by normalized path.
- Visit only regular files.
- Never follow symlinks.
- Skip special files.
- Skip inaccessible directories.
- Allow excluded directories to stop recursion before entering them.

### Requirements for This Step

- Traversal order is deterministic.
- No full pending-file list is stored.
- No full disk list is held globally in memory.
- Permission failures are logged.
- Deleted entries are skipped safely.
- Symlinks are not followed.
- Special files are not opened.
- Callback can stop traversal.
- Paths with spaces and Unicode work.

### Completion Criteria

Tests cover:

- one file;
- nested directory;
- empty directory;
- symlink;
- inaccessible directory;
- deterministic repeated order;
- callback-requested stop.

---

## Step 7 — Implement File Metadata Reading

### Purpose

Provide reliable metadata for cache identity and file-change detection.

### Files and Classes

```text
include/scan/FileMetadataProvider.h
src/scan/FileMetadataProvider.cpp
```

### Types

```cpp
struct FileMetadata {
    std::filesystem::path normalizedPath;
    uint64_t deviceId;
    uint64_t inode;
    uintmax_t size;
    int64_t modificationTime;
};
```

### `FileMetadataProvider`

Must expose:

```cpp
class FileMetadataProvider {
public:
    FileMetadata read(const std::filesystem::path& path) const;
    bool sameFileState(
        const FileMetadata& first,
        const FileMetadata& second
    ) const;
};
```

### Requirements for This Step

- Path is normalized consistently.
- Device ID is read.
- Inode is read.
- Size is read.
- Modification time is read.
- Missing files return a controlled error.
- Metadata comparison checks all relevant values.
- Metadata logic is not duplicated in cache or scan classes.

### Completion Criteria

- Tests verify unchanged metadata equality.
- Tests verify size change.
- Tests verify modification-time change.
- Tests verify replacement at the same path.
- Missing-file handling is tested.

---


## Step 8 — Implement File Cache

### Purpose

Avoid rescanning unchanged files.

### Cache Hit Conditions

Reuse a cached verdict only when all values match:

```text
normalized path
device ID
inode
size
modification time
signature version
successful cached verdict
```

### Requirements for This Step

- Unchanged file produces a cache hit.
- Changed size causes rescan.
- Changed modification time causes rescan.
- Changed inode causes rescan.
- Changed signature version causes rescan.
- Failed scans are not reused.
- Metadata is checked before and after scanning.
- Changed-during-scan results are not cached.
- One retry is allowed for an unstable file.
- Cache hits update session statistics.
- Cache lookup occurs after exclusion check.
- Cache records persist across application restart.

### Completion Criteria

Tests cover:

- unchanged file hit;
- changed file miss;
- same path with replacement file;
- signature version invalidation;
- failed scan not reused;
- changed-during-scan behavior;
- restart persistence.

---


## Step 9 — Implement Basic Scan Sessions and ScanService

### Purpose

Connect traversal, cache, scanning, persistence, logging, and summaries.

### Files and Classes

```text
include/scan/ScanService.h
src/scan/ScanService.cpp
```

### `ScanService`

Must receive:

```text
FileTraverser
FileScanner
FileMetadataProvider
ScanSessionRepository
SignatureRepository
CacheRepository
ExclusionService
QuarantineService
Logger
```

### Initial Public Methods

```cpp
class ScanService {
public:
    int scanPath(const std::filesystem::path& path);
    int scanAll();
    int requestStop();
    int resume();
};
```

### File Processing Order

For each file:

1. normalize path;
2. check exclusion;
3. read metadata before scan;
4. check cache;
5. scan when cache cannot be reused;
6. read metadata after scan;
7. reject unstable result when metadata changed;
8. quarantine malicious file;
9. update cache;
10. update counters;
11. update checkpoint;
12. check stop request;
13. continue or stop.

### Requirements for This Step

- Session is created before traversal.
- Only one active session is allowed.
- Path and scan type are stored.
- File errors do not stop the entire scan.
- A file becomes completed only after all handling finishes.
- Checkpoint is updated last.
- Session becomes completed after traversal ends.
- Fatal initialization failure marks the session failed.
- Scan summary is printed.
- `scanAll()` reuses `scanPath()` with the configured root.

### Example

```bash
scanner scan --path ./test-data
```

Output:

```text
Scan completed
Files scanned: 5
Cache hits: 0
Malicious files: 1
Errors: 0
```

### Completion Criteria

- One-file scan works end to end.
- Directory scan works end to end.
- Session data is stored correctly.
- Counters are correct.
- One unreadable file does not stop remaining files.

---

## Step 10 — Implement Stop and Resume

### Purpose

Meet the stop-and-continue requirement without storing every pending file.

### Stop Behavior

```text
scanner stop
```

Must:

1. find the running session;
2. set status to `stop_requested`;
3. return immediately.

The scanning process must:

1. finish the current file;
2. finish quarantine/cache/log/checkpoint updates;
3. see `stop_requested`;
4. mark session `stopped`;
5. exit before processing the next file.

### Resume Behavior

```text
scanner resume
```

Must:

1. load the latest stopped session;
2. mark it running;
3. traverse the original root using the same deterministic order; alpahbetic
4. skip paths less than or equal to `last_completed_path`;
5. continue normal scanning.

### Requirements for This Step

- No mid-file interruption occurs.
- Stop request is persisted.
- Checkpoint is saved before stopped status.
- Resume uses the original normalized root.
- Resume uses identical traversal ordering.
- Deleted checkpoint file does not block progress.
- Completed sessions cannot resume.
- Missing stopped session returns a clear error.
- Only one active scan exists.
- Pause-time filesystem behavior is documented.

### Pause-Time Behavior

- New or modified files after the checkpoint can be processed during resume.
- Deleted files after the checkpoint are skipped.
- New or modified files before the checkpoint wait for the next new scan.
- The next new scan catches them through cache comparison.

### Completion Criteria

Tests prove:

- stop happens after current file;
- resume starts after checkpoint;
- restart does not lose stop state;
- deleted checkpoint path does not block resume;
- duplicate scans are rejected.

---




## Step 11 — Implement Quarantine

### Purpose

Move malicious files safely and support management commands.

### Files and Classes

```text
include/quarantine/QuarantineService.h
src/quarantine/QuarantineService.cpp
```

### Public Methods

```cpp
class QuarantineService {
public:
    QuarantineItem quarantine(
        const std::filesystem::path& path,
        const FileMetadata& metadata,
        std::optional<int64_t> signatureId
    );

    std::vector<QuarantineItem> list() const;
    void restore(const std::string& id);
    void remove(const std::string& id);
};
```

### Quarantine Flow

1. generate UUID;
2. create destination `quarantine/<uuid>.bin`;
3. move file with `rename`;
4. if cross-filesystem rename fails, copy then delete original;
5. store original metadata and path;
6. log action.

### Restore Rules

- record must exist;
- status must be `quarantined`;
- quarantine file must exist;
- original parent directory must exist;
- original destination must not already exist;
- never overwrite silently.

### Delete Rules

- record must exist;
- status must be `quarantined`;
- only the exact quarantine file may be removed;
- status becomes `deleted`.

### Requirements for This Step

- Malicious file leaves original location.
- Quarantine names cannot collide.
- Original metadata is stored.
- Matched signature ID is stored.
- Quarantine directory is always excluded.
- Restore never overwrites.
- Repeated restore/delete fails safely.
- Partial failures are logged.
- Database and filesystem states remain as consistent as possible.

### Completion Criteria

Tests cover:

- quarantine;
- list;
- restore;
- restore conflict;
- delete;
- repeated delete;
- missing quarantine file;
- path with spaces.

---

## Step 12 — Implement Exclusions

### Purpose

Allow users to skip exact files and full directory trees.

### Files and Classes

```text
include/exclusions/ExclusionService.h
src/exclusions/ExclusionService.cpp
```

### Public Methods

```cpp
class ExclusionService {
public:
    std::vector<Exclusion> list() const;
    void add(const std::filesystem::path& path);
    void remove(int64_t id);
    bool isExcluded(const std::filesystem::path& path) const;
};
```

### Exclusion Rules

- Exact file exclusion skips that file.
- Directory exclusion skips the entire subtree.
- Internal scanner paths are mandatory exclusions.
- Path-component comparison is used.
- Raw string-prefix matching is forbidden.

### Example

Excluded:

```text
/Users/ronen/test
```

Excluded:

```text
/Users/ronen/test/file.txt
```

Not excluded:

```text
/Users/ronen/testing/file.txt
```

### Requirements for This Step

- Exclusions persist.
- Duplicate exclusions are rejected clearly.
- Relative paths are normalized.
- Missing paths may still be stored.
- Directory recursion stops before entering excluded directories.
- Internal exclusions cannot be removed.
- Exclusions are checked before cache and scan.
- Add/remove actions are logged.

### Completion Criteria

Tests cover:

- exact file exclusion;
- directory subtree exclusion;
- similar-prefix non-match;
- duplicate add;
- remove;
- persistence;
- internal exclusion protection.

---

## Step 13 — Complete Signature Management Commands

### Purpose

Expose signature configuration through the CLI.

### Required Commands

```bash
scanner signatures list
scanner signatures add <value>
scanner signatures remove <id>
```

### Required Transaction

Every successful add or remove must perform:

```text
signature table change
+
signature_version increment
```

in one SQLite transaction.

### Requirements for This Step

- Add works without recompilation.
- Remove works without recompilation.
- List shows all active signatures.
- Empty values are rejected.
- Duplicates are rejected.
- Invalid IDs return clear errors.
- Signature version increments exactly once per change.
- Older cache results become invalid.
- Signature actions are logged.
- Matching is documented as literal byte matching.

### Completion Criteria

Tests cover:

- add;
- list;
- remove;
- duplicate;
- empty value;
- invalid ID;
- persistence;
- cache invalidation.

---

## Step 14 — Complete User Output and Error Handling

### Purpose

Make every command clear and consistent for the user.

### Required Scan Summary

```text
Scan completed

Path: /Users/ronen/Documents
Files scanned: 125
Cache hits: 970
Malicious files: 2
Excluded files: 15
Skipped files: 3
Errors: 4
Duration: 12.4 seconds
```

### Requirements for This Step

- Completed scan prints final summary.
- Stopped scan prints partial summary.
- Failed scan prints fatal reason.
- Counters match database values.
- Exit codes are consistent.
- No uncaught exception reaches the terminal.
- File-level errors remain non-fatal.
- Fatal database/configuration errors stop safely.
- Important information is logged and shown when appropriate.

### Completion Criteria

- All commands have clear output.
- Exit codes are tested.
- Counters are verified in integration tests.
- Expected failures do not print raw internal exceptions.

---

## Step 15 — Implement `scan --all`

### Purpose

Scan the configured full-scan root using the same engine.

### Required Behavior

```bash
scanner scan --all
```

must call:

```cpp
scanService.scanPath(config.fullScanRoot);
```

with scan type `all`.

### Requirements for This Step

- No separate full-scan engine exists.
- Cache works.
- Exclusions work.
- Stop/resume works.
- Quarantine works.
- Permission errors are logged.
- Symlinks and special files are skipped.
- Development uses a safe test root.
- README explains the production root and permissions.

### Completion Criteria

- `scan --all` works on a test root.
- Behavior matches `scan --path`.
- All scanner features work during full scan.

---

## Step 16 — Implement macOS Startup Integration

### Purpose

Run `scanner scan --all` automatically when the user logs in.

### Files

```text
launchd/com.sentinel.scanner.plist
```

### Required LaunchAgent Configuration

- absolute executable path;
- `ProgramArguments`;
- `RunAtLoad`;
- standard output path;
- standard error path.

### Required Command

```text
scanner scan --all
```

### Requirements for This Step

- Uses a LaunchAgent.
- Runs with user permissions.
- Does not require root.
- Prevents duplicate active scans.
- Startup failures are logged.
- Install and uninstall commands are documented.
- Manual CLI commands still work.
- Difference between login startup and system boot is documented.

### Completion Criteria

- Login starts a scan.
- A scan session is created.
- Duplicate invocation is rejected.
- Manual scanning still works after installation.

---

## Step 17 — Tests, README, and Final Demo

### Purpose

Prove every demand and document the final design.

### Required Unit Tests

- command parsing;
- logger;
- repository operations;
- signature matching;
- chunk-boundary matching;
- deterministic traversal;
- metadata comparison;
- exclusion matching;
- cache hit logic;
- session status transitions.

### Required Integration Tests

- scan one file;
- scan directory;
- cache hit;
- cache invalidation;
- quarantine;
- restore;
- delete;
- stop;
- resume;
- permission error;
- exclusion subtree;
- persistence after restart.

### Required Demo Flow

```bash
scanner signatures add "evil_payload"
scanner exclusions add ./test-data/excluded
scanner scan --path ./test-data
scanner quarantine list
scanner quarantine restore <id>
scanner scan --path ./test-data
scanner stop
scanner resume
scanner scan --all
```

### README Must Explain

- build instructions;
- dependencies;
- commands;
- architecture;
- database location;
- log location;
- quarantine location;
- detection algorithm;
- cache rules;
- signature version;
- quarantine rules;
- exclusion rules;
- stop/resume behavior;
- pause-time filesystem changes;
- permission behavior;
- symlink behavior;
- full-scan root;
- LaunchAgent setup;
- test commands;
- limitations.

### Requirements for This Step

- Every assignment demand has a test.
- Tests use temporary directories.
- Tests never scan the real machine.
- Test signatures are harmless.
- README matches actual behavior.
- Demo works from a clean checkout.
- Limitations are stated honestly.

### Completion Criteria

- All tests pass.
- README instructions work.
- Demo proves all seven demands.
- LaunchAgent setup is reproducible.
- Final requirement checklist is complete.

---

# 5. Requirement Coverage Matrix

| Assignment Demand | Main Implementation Steps |
|---|---|
| 1. Cache unchanged files | Steps 4, 7, 8, 10, 13 |
| 2. Scan path or full root | Steps 1, 6, 8, 15 |
| 3. Quarantine, restore, delete | Steps 4, 8, 11 |
| 4. Change signatures without recompilation | Steps 4, 5, 10, 13 |
| 5. Logging | Steps 3, 8, 11, 12, 13, 14 |
| 6. Stop and resume | Steps 4, 6, 8, 9 |
| 7. Exclusions | Steps 2, 6, 8, 12 |
| Automatic startup | Steps 15 and 16 |

---

# 6. Final Definition of Done

The assignment is finished only when:

- the project builds on macOS;
- all CLI commands parse correctly;
- one file can be scanned;
- one directory can be scanned recursively;
- the configured full root can be scanned;
- only regular files are scanned;
- symlinks are ignored;
- special files are ignored;
- permission failures are logged;
- scanning continues after file-level errors;
- signatures are managed without recompilation;
- matching works across chunk boundaries;
- unchanged files use cache;
- signature changes invalidate cache;
- malicious files are quarantined;
- quarantine items can be listed;
- quarantine items can be restored safely;
- quarantine items can be deleted;
- files can be excluded;
- directory trees can be excluded;
- a scan stops after the current file;
- a stopped scan resumes after the checkpoint;
- only one scan runs at a time;
- scan summaries are printed;
- important events are logged;
- scanner-owned files are always excluded;
- `scan --all` reuses the normal scan engine;
- macOS LaunchAgent startup works;
- tests cover every assignment demand;
- README documents behavior and limitations.

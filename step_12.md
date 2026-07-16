# Step 12 — Exclusions

## What Was Done

Implemented `ExclusionService` with path-component comparison and wired it into `ScanService` (checked before cache/scan) and `Application` (CLI commands).

At startup, `main.cpp` seeds the scanner's own data directory into the exclusions table so the scanner never scans its own DB/logs/quarantine.

### Files Created / Modified

```
include/exclusions/ExclusionService.h   (new)
src/exclusions/ExclusionService.cpp     (new)
include/scan/ScanService.h              (added ExclusionService& param)
src/scan/ScanService.cpp                (exclusion check at top of processFile)
include/app/Application.h              (added ExclusionService& param)
src/app/Application.cpp                (wired ExclusionList/Add/Remove)
src/main.cpp                            (construct ExclusionService, seed data dir)
CMakeLists.txt                          (added ExclusionService.cpp)
```

---

## Test Instructions

### 1 — Setup

```bash
./build.sh
rm -f scanner-data/scanner.db

./build/scanner signatures add "evil_payload"
```

---

### 2 — Startup auto-exclusion

The scanner seeds its own data directory at startup. Verify it's there on first run:

```bash
./build/scanner exclusions list
```

Expected:
```
ID    Type       Path
1     directory  /.../.../scanner-data
```

On subsequent runs, the data dir is already there — no duplicate is added:

```bash
./build/scanner exclusions list   # still shows 1 entry
```

---

### 3 — Add a directory exclusion

```bash
./build/scanner exclusions add test-data/a
./build/scanner exclusions list
```

Expected:
```
ID    Type       Path
1     directory  .../scanner-data
2     directory  .../test-data/a
```

---

### 4 — Directory exclusion is honored during scan

test-data/a has 5 files (one malicious). With a/ excluded, the scan should skip them all:

```bash
./build/scanner scan --path test-data
```

Expected: files from a/ are not counted. Malicious file inside a/ is NOT detected (it's excluded).

---

### 5 — Similar-prefix non-match (key correctness test)

Excluding `test-data/a` must NOT exclude `test-data/b` or `test-data/ab`-style paths.
Verify: the scan above does count files from b/, c/, d/, e/.

---

### 6 — Add a file exclusion

```bash
./build/scanner exclusions add test-data/bad.txt
./build/scanner exclusions list
# bad.txt appears as type "file"

./build/scanner scan --path test-data
# bad.txt is skipped
```

---

### 7 — Duplicate rejection

```bash
./build/scanner exclusions add test-data/a
# Error: Exclusion already exists: .../test-data/a
```

---

### 8 — Remove exclusion

```bash
./build/scanner exclusions remove 2    # removes test-data/a
./build/scanner exclusions list        # a is gone

./build/scanner scan --path test-data  # a/ is now scanned
```

---

### 9 — Remove non-existent id

```bash
./build/scanner exclusions remove 99
# Error: Exclusion not found: 99
```

---

### 10 — DB verification

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT id, path, is_directory FROM exclusions;"
```

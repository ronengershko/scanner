# Step 5 — Signature Storage and One-File Scanning

## What Was Done

Added `ScanTypes.h` (shared types), `FileScanner` (binary file scanning), and `SignatureService` (signature management). Signature CLI commands now fully work.

### Files Created / Modified

```
include/scan/ScanTypes.h               (new)
include/scan/FileScanner.h             (new)
src/scan/FileScanner.cpp               (new)
include/signatures/SignatureService.h  (new)
src/signatures/SignatureService.cpp    (new)
include/app/Application.h             (updated — takes SignatureService&)
src/app/Application.cpp               (updated — signature commands wired)
src/main.cpp                          (updated — constructs SignatureRepository + SignatureService)
CMakeLists.txt                        (updated)
```

### Design

**FileScanner — chunk boundary matching:**

Files are read in 64KB chunks. To catch signatures that span two chunks, a tail of `maxSignatureLength - 1` bytes is kept from the previous chunk and prepended to the next. Searching always happens in `tail + new_chunk`. After each search the window is trimmed back to just the tail.

**SignatureService — add/remove atomicity:**

`add` and `remove` each call `incrementVersion()` immediately after the DB change. Both version and signature are in the same `sqlite3*` connection so they are consistent. Full transaction wrapping (BEGIN/COMMIT) is deferred to step 13 where it is explicitly required.

**Type separation:**

- `SignatureRecord` (in `SignatureRepository.h`) — DB representation, value is `std::string`
- `Signature` (in `ScanTypes.h`) — scanner representation, value is `std::vector<std::byte>`
- `SignatureService::loadForScanning()` converts between them

---

## How to Build

```bash
./build.sh
```

---

## How to Test

### List when empty
```bash
./build/scanner signatures list
# Expected: No signatures configured.
```

### Add and list
```bash
./build/scanner signatures add "evil_payload"
./build/scanner signatures add "dangerous_text"
./build/scanner signatures list
# Expected: both signatures shown with IDs
```

### Signature version increments on each change
```bash
sqlite3 scanner-data/scanner.db "SELECT value FROM scanner_metadata WHERE key='signature_version';"
# Starts at 1, increments by 1 for each add/remove
```

### Duplicate rejected
```bash
./build/scanner signatures add "evil_payload"
echo $?   # expect 1 with UNIQUE constraint error
```

### Remove
```bash
./build/scanner signatures remove <id>
./build/scanner signatures list
# Expected: removed signature no longer listed
```

### Persists across restarts
```bash
./build/scanner signatures list   # run again after prior commands
# Expected: same signatures still present
```

### FileScanner logic (verified in step 9 when scan is wired)
The scanner reads in 64KB chunks with `maxSigLen - 1` byte overlap between chunks.
A signature split exactly across a chunk boundary will still be detected.

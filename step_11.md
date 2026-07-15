# Step 11 — Quarantine

## What Was Done

Implemented `QuarantineService` and wired it into `ScanService` (auto-quarantine on malicious) and `Application` (CLI commands).

### Files Created / Modified

```
include/quarantine/QuarantineService.h   (new)
src/quarantine/QuarantineService.cpp     (new)
include/scan/ScanService.h               (added QuarantineService& param)
src/scan/ScanService.cpp                 (call quarantine when malicious)
include/app/Application.h               (added QuarantineService& param)
src/app/Application.cpp                 (wired QuarantineList/Restore/Delete)
src/main.cpp                             (construct QuarantineRepository + Service)
CMakeLists.txt                           (added QuarantineService.cpp)
```

---

## How Quarantine Works

### On scan detection
When `FileScanner` returns `Verdict::Malicious`, `ScanService` calls:
```
QuarantineService::quarantine(file, metadata, signatureId)
```
Which:
1. Generates a UUID
2. Moves the file to `scanner-data/quarantine/<uuid>.bin` (falls back to copy+delete on cross-filesystem)
3. Stores original path, metadata, and matched signature ID in the DB with status `quarantined`

### Restore rules
- Item must exist and have status `quarantined`
- The `.bin` file must still exist in quarantine
- The original parent directory must exist
- The original path must not already be occupied (no silent overwrite)

### Delete rules
- Item must exist and have status `quarantined`
- Removes the `.bin` file from disk
- Sets status to `deleted` in the DB

---

## Test Instructions

### 1 — Setup

```bash
./build.sh
rm -f scanner-data/scanner.db
rm -rf scanner-data/quarantine && mkdir -p scanner-data/quarantine

./build/scanner signatures add "evil_payload"

# Create test files
mkdir -p test-data
echo "clean content"        > test-data/good.txt
echo "contains evil_payload" > test-data/bad.txt
```

---

### 2 — Scan and observe auto-quarantine

```bash
./build/scanner scan --path test-data
```

Expected output includes:
```
MALICIOUS: .../test-data/bad.txt
Scan completed
Files scanned: 2
Malicious files: 1
```

Verify the file was moved away from its original location:

```bash
ls test-data/bad.txt
# → ls: test-data/bad.txt: No such file or directory
```

Verify the `.bin` file is in the quarantine directory:

```bash
ls scanner-data/quarantine/
# → <uuid>.bin
```

---

### 3 — List quarantine

```bash
./build/scanner quarantine list
```

Expected:
```
ID                                   Status       Original Path
<uuid>  quarantined  .../test-data/bad.txt
```

Get the ID and store it for the next steps:

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT id, status, original_path FROM quarantine_items;"
```

---

### 4 — Restore

```bash
./build/scanner quarantine restore <id>
# → Restored.
```

Verify the file is back:

```bash
cat test-data/bad.txt
# → contains evil_payload
```

Try restoring again (should fail):

```bash
./build/scanner quarantine restore <id>
# → Error: Item is not quarantined (status=restored)
```

---

### 5 — Scan again after restore (file gets re-quarantined)

```bash
./build/scanner scan --path test-data/bad.txt
```

Expected: detected again, quarantined again. A new entry appears in the quarantine list.

---

### 6 — Delete a quarantine item

```bash
./build/scanner quarantine list   # get a quarantined id
./build/scanner quarantine delete <id>
# → Deleted.
```

Verify the `.bin` file is gone:

```bash
ls scanner-data/quarantine/
```

Verify DB status:

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT id, status, original_path FROM quarantine_items;"
```

Expected: that entry shows `deleted`, the `.bin` file is absent.

Try deleting again (should fail):

```bash
./build/scanner quarantine delete <id>
# → Error: Item is not quarantined (status=deleted)
```

---

### 7 — Restore conflict

Create a file at the original path, then try to restore:

```bash
echo "imposter" > test-data/bad.txt
./build/scanner quarantine restore <still-quarantined-id>
# → Error: File already exists at original path: .../test-data/bad.txt
```

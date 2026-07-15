# Step 10 — Stop and Resume

## What Was Done

Implemented `resume()` in `ScanService` and added a test-delay env var.  
Fixed `updateCounters` to **accumulate** (+=) so counts from a stopped run carry over after resume.

### Files Modified

```
src/scan/ScanService.cpp                (resume implemented, delay added)
src/database/ScanSessionRepository.cpp  (updateCounters uses += instead of =)
```

---

## How Stop Works

`scanner stop` sets the running session status to `stop_requested` in the DB and returns immediately. The scan loop checks the status **before each file**. When it sees `stop_requested` it finishes the current file (cache, checkpoint, counters), marks the session `stopped`, and exits.

## How Resume Works

`scanner resume` loads the latest `stopped` session, marks it `running`, re-traverses the **same canonical path** in the same alphabetical order, and skips every file whose path is `<=` the stored `last_completed_path`. The checkpoint comparison works because the traversal order is deterministic.

## Test Delay

`SCANNER_FILE_DELAY_MS=<ms>` adds an artificial sleep after each file so you have time to issue `scanner stop`. Has no effect when unset.

---

## Test Instructions

### 1 — Setup

```bash
./build.sh

# Fresh database
rm -f scanner-data/scanner.db

# Add the detection signature
./build/scanner signatures add "evil_payload"
```

The test-data directory already has 25 files, 5 of which contain `evil_payload`:
- `test-data/a/file3.txt`
- `test-data/b/file2.txt`
- `test-data/c/file4.txt`
- `test-data/d/file1.txt`
- `test-data/e/file5.txt`

---

### 2 — Start a slow scan and stop it mid-way

In one terminal:

```bash
SCANNER_FILE_DELAY_MS=200 ./build/scanner scan --path test-data &
SCAN_PID=$!
sleep 3
./build/scanner stop
wait $SCAN_PID
```

Expected: scan processes ~15 files (3 seconds / 200ms), then stops. You will see `Scan stopped` with a partial summary.

---

### 3 — Inspect the session in the DB

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT id, status, scanned_files, cache_hits, malicious_files,
          last_completed_path
   FROM scan_sessions
   ORDER BY id DESC LIMIT 1;"
```

Expected:
- `status = stopped`
- `scanned_files` shows how many were scanned before the stop
- `last_completed_path` shows the last file that completed

---

### 4 — Inspect the cache before resume

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT COUNT(*) as cached_files FROM file_cache;"
```

Expected: a partial count — fewer than 25, matching the files processed before the stop.

---

### 5 — Resume the scan

```bash
./build/scanner resume
```

Expected: picks up after the checkpoint, scans the remaining files, prints `Scan completed`.

---

### 6 — Verify the same session was used and counters are cumulative

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT id, status, scanned_files, cache_hits, malicious_files, error_files
   FROM scan_sessions
   ORDER BY id DESC LIMIT 1;"
```

Expected:
- **Same `id`** as the stopped session from step 3
- `status = completed`
- `scanned_files + cache_hits = 25` (every file accounted for across both runs)
- `malicious_files = 5`

---

### 7 — Verify the cache is now complete

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT COUNT(*) as cached_files FROM file_cache;"
```

Expected: **25** — all files are cached.

```bash
sqlite3 scanner-data/scanner.db \
  "SELECT path, verdict FROM file_cache WHERE verdict = 'malicious';"
```

Expected: 5 rows — the 5 malicious files.

---

### 8 — Error cases

```bash
# Stop when nothing is running
./build/scanner stop
# → "No running scan to stop."

# Resume when nothing is stopped
./build/scanner resume
# → "No stopped scan to resume."

# Try to start a second scan while one is running
SCANNER_FILE_DELAY_MS=500 ./build/scanner scan --path test-data &
./build/scanner scan --path test-data
# → "A scan is already running."
kill %1
```

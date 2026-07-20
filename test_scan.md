# Scan Manual Test

## Setup

```bash
# Build
cmake --build build

# Add a test signature 
./build/scanner signatures add virus
./build/scanner signatures list
# Expected: ID 1  Value  virus

# Create test directories
mkdir -p ~/scan-test/subdir

# Create files
echo "hello world" > ~/scan-test/clean.txt
echo "bad_virus" > ~/scan-test/malicious.txt
echo "another clean file" > ~/scan-test/subdir/clean2.txt
```

---

## Test 1 — scan --path (single directory)

```bash
./build/scanner scan --path ~/scan-test
```

Expected output:
```
MALICIOUS: /Users/<you>/scan-test/malicious.txt
Scan completed
Path: /Users/<you>/scan-test
Files scanned: 3
Malicious files: 1
```

Verify the malicious file was quarantined:
```bash
ls ~/scan-test/
# malicious.txt should be gone

./build/scanner quarantine list
# should show malicious.txt as quarantined
```

---

## Test 2 — scan --all (full scan from configured root)

```bash
# Set the scan root
./build/scanner config set-root ~/scan-test

# Run full scan (malicious.txt already quarantined, so 0 malicious expected)
./build/scanner scan --all
```

Expected output:
```
Scan completed
Path: /Users/<you>/scan-test
Files scanned: 2
Malicious files: 0
```

---

## Test 3 — cache hit on second scan

Run `scan --path` again on the same directory:

```bash
./build/scanner scan --path ~/scan-test
```

Expected output:
```
Scan completed
Files scanned: 0
Cache hits: 2
```

All files returned from cache — no re-scanning.

---

## Test 4 — stop and resume

```bash
# Add more files to make the scan take longer
for i in $(seq 1 100); do echo "clean" > ~/scan-test/file_$i.txt; done

# Start scan in background
./build/scanner scan --path ~/scan-test &

# Stop it
./build/scanner stop
# Expected: Stop requested.

# Resume
./build/scanner resume
# Expected: Scan completed (picks up from checkpoint)
```

---

## Test 5 — scan --all with no root set (error case)

```bash
# Delete the DB to reset state
rm scanner-data/scanner.db

./build/scanner scan --all
```

Expected:
```
Error: Scan root not set. Use 'scanner config set-root <path>'.
```

---

## Check sessions history

```bash
./build/scanner sessions
# Shows last 10 scan sessions with path, type, status, file counts
```

---

## Cleanup

```bash
rm -rf ~/scan-test
```

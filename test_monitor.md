# Monitor Manual Test

## Setup

```bash
# Build
cmake --build build

# Create watched directory
mkdir -p /tmp/watch-test

# Add watch path
./build/scanner watch add /tmp/watch-test

# Verify
./build/scanner watch list
```

## Start the monitor

In terminal 1:
```bash
./build/scanner monitor
```

Expected output:
```
Monitoring: /tmp/watch-test
(Ctrl+C to stop)
```

## Test 1 — Clean file (no alert)

In terminal 2:
```bash
echo "hello world" > /tmp/watch-test/clean.txt
```

Expected: no output in monitor terminal, nothing in log.

## Test 2 — Malicious file (detected + quarantined)

```bash
echo "bad_virus payload" > /tmp/watch-test/evil.txt
```

Expected in monitor terminal:
```
MALICIOUS: /tmp/watch-test/evil.txt
```

Expected in log:
```bash
tail -4 scanner-data/scanner.log
# WARNING Malicious: /tmp/watch-test/evil.txt
# INFO Quarantined: /tmp/watch-test/evil.txt id=<uuid>
```

Verify the file was removed:
```bash
ls /tmp/watch-test/
# evil.txt should be gone
```

## Test 3 — Modify an existing file

```bash
echo "bad_virus" >> /tmp/watch-test/clean.txt
```

Expected: FSEvents fires on modification, file is scanned and quarantined.

## Test 4 — Add a new watch path while monitor is running (no restart needed)

While monitor is still running in terminal 1, in terminal 2:
```bash
mkdir -p /tmp/watch-test2
./build/scanner watch add /tmp/watch-test2
```

Expected in terminal 2:
```
Watch path added: /tmp/watch-test2
Monitor reloaded.
```

Monitor restarts automatically and now watches both paths:
```
Monitoring: /tmp/watch-test
Monitoring: /tmp/watch-test2
(Ctrl+C to stop)
```

Drop a file in the new path:
```bash
echo "bad_virus" > /tmp/watch-test2/evil2.txt
```

Expected: evil2.txt caught and quarantined.

## Test 5 — Remove a watch path while monitor is running

```bash
./build/scanner watch list           # note the id of watch-test2
./build/scanner watch remove <id>
```

Expected: "Monitor reloaded." — monitor restarts watching only the remaining path.

## Check quarantine

```bash
./build/scanner quarantine list
```

## Cleanup

```bash
./build/scanner watch remove 1
./build/scanner watch list   # should be empty
```

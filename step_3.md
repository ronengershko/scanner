# Step 3 — Central Logging

## What Was Done

Added `Logger` — writes timestamped lines to the log file. Wired it into `Application` (logs startup and invalid commands). `main.cpp` constructs the logger and passes it to `Application` by reference so future services can share the same instance.

### Files Created / Modified

```
include/logging/Logger.h       (new)
src/logging/Logger.cpp         (new)
include/app/Application.h      (updated — constructor now takes Logger&)
src/app/Application.cpp        (updated — logs startup and parse errors)
src/main.cpp                   (updated — constructs Logger before Application)
CMakeLists.txt                 (updated — added Logger.cpp)
```

### Design

- Opens the log file in **append** mode (`std::ios::app`) — multiple runs accumulate in the same file.
- Flushes after every write so the log is always readable, even if the process crashes mid-scan.
- `Application` takes `Logger&` (not by value) so the same instance will be shared with all future services.
- When logging an invalid command error, only the first line of the exception message is written — the usage text is printed to stderr but kept out of the log.

### Log format

```
2026-07-14 18:08:14 INFO Scanner started
2026-07-14 18:08:14 ERROR Invalid command: Unknown command: badcmd
```

---

## How to Build

```bash
./build.sh
```

---

## How to Test

### Log file created, startup logged

```bash
rm -f scanner-data/scanner.log
./build/scanner stop
cat scanner-data/scanner.log
# Expected: one INFO line "Scanner started"
```

### Multiple runs append (do not overwrite)

```bash
./build/scanner stop
./build/scanner stop
cat scanner-data/scanner.log | wc -l
# Expected: 2 lines (each run adds one)
```

### Invalid command logged as ERROR (one clean line)

```bash
./build/scanner badcmd 2>/dev/null || true
grep "ERROR" scanner-data/scanner.log
# Expected: single ERROR line, no usage text mixed in
```

### Check format

```bash
cat scanner-data/scanner.log
# Each line: YYYY-MM-DD HH:MM:SS LEVEL message
```
